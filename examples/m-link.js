class MLink {
  /*
   * Construct an MLink object to talk to an M-Link Lite robot controller
   */
  constructor (uri, options = {}) {
    this.onmessage = undefined
    this.onopen = undefined
    this.onclose = undefined
    this._status = 'waiting'
    this._awaiting = []

    if (options.onmesage) {
      this.onmessage = options.onmessage
    }
    if (options.onopen) {
      this.onopen = options.onopen
    }
    if (options.onclose) {
      this.onclose = options.onclose
    }
    if (options.failsafes) {
      this._failsafes = options.failsafes
    }

    if (window.WebSocket) {
      this._ws = new WebSocket(uri)
    } else if (window.MozWebSocket) {
      this._ws = new MozWebSocket(uri)
    } else {
      console.error('WebSocket not supported')
      return
    }

    const localthis = this
    this._ws.onmessage = function (evt) {
      let obj = JSON.parse(evt.data)

      if (obj.status) {
        localthis._status = obj.status
      }

      // If we queued up a function handle this response then call it
      const resolve = localthis._awaiting.shift()
      if (resolve)
      {
        resolve(obj)
      }

      if (localthis.onmessage) {
        localthis.onmessage(obj)
      }
    }
    this._ws.onopen = function () {
      localthis._status = open
      if (localthis.onopen) {
        localthis.onopen()
      }
    }
    this._ws.onclose = function (evt) {
      localthis._status = closed
      if (localthis.onclose) {
        localthis.onclose(evt.code, evt.reason)
      }
    }
  }

  /*
   * Open the WebSocket connection
   */
  async begin() {
    // Send initial settings query
    const settings = await this.getSettings()

    if (this._failsafes) {
      await this.setFailsafes(this._failsafes)
    } else {
      // Once we know (or guess) the number of channels, set sensible failsafes
      let channels = 6
      if (settings && settings.channels) {
        channels = parseInt(settings.channels)
      }
      const failsafes = new Array(channels)
      failsafes.fill(1500)
      await this.setFailsafes(failsafes)
    }
  }

  /*
   * Get the status of the connection
   */
  get status() {
    // TODO: Implement this
    return this._status
  }

  /*
   * Wrapper around WebSocket send
   * Returns only once a response is received
   */
  async _send (msg) {
    const localthis = this
    const promise = new Promise((resolve, reject) => {
      // Send the message whose response will resolve this promise
      localthis._ws.send(JSON.stringify(msg))

      // onmessage will call resolve when the relevant response comes in
      localthis._awaiting.push(resolve)
 
      // Reject if there is no response after 300 ms
      setTimeout(() => reject({status: 'timed out'}), 300)
    })

    // Wait for the promise to be resolved or rejected
    return await promise
  }

  /*
   * Set the failsafes for each channel
   */
  async setFailsafes (failsafes) {
    return await this._send(
      {
        failsafes: failsafes
      }
    )
  }

  /*
   * Set the pulsewidth for each servo
   */
  async setServos (servos) {
    return await this._send(
      {
        servos: servos
      }
    )
  }

  /*
   * Update settings
   */
  async updateSettings (settings) {
    return await this._send(
      {
        "settings" : settings
      }
    )
  }

  /*
   * Reset default settings
   */
  async resetSettings () {
    return await this._send(
      {
        reset_settings : "sgnittes_teser"
      }
    )
  }

  /*
   * Reboot
   */
  async reboot () {
    this._send(
      {
        reboot : "toober"
      }
    )
    return {
      status: 'rebooting'
    }
  }

  /*
   * Query Battery
   */
  async getBatteryVoltage () {
    const result = await this._send(
      {
        query : "battery"
      }
    )
    if (result && result.status && result.status === 'ok') {
      return parseInt(result.battery)
    }
    return 0
  }

  /*
   * Query Failsafes
   */
  async getFailsafes () {
    const result = await this._send(
      {
        query : "failsafes"
      }
    )
    if (result && result.status && result.status === 'ok') {
      return result.failsafes.map(pw => parseInt(pw))
    }
    return []
  }

  /*
   * Query Settings
   */
  async getSettings () {
    const result = await this._send(
      {
        query : "settings"
      }
    )
    if (result && result.status && result.status === 'ok') {
      return result.settings
    }
    return {}
  }
}

