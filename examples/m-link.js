class MLink {
  /*
   * Construct an MLink object to talk to an M-Link Lite robot controller
   */
  constructor (uri, options = {}) {
    this.onmessage = undefined
    this.onopen = undefined
    this.onclose = undefined
    this._status = 'waiting'

    if (options.onmesage) {
      this.onmessage = options.onmessage
    }
    if (options.onopen) {
      this.onopen = options.onopen
    }
    if (options.onclose) {
      this.onclose = options.onclose
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
  begin() {
    // Send initial settings query
    this._ws.send(
      JSON.stringify(
        {
          query: "settings"
        }
      )
    )
  }

  /*
   * Get the status of the connection
   */
  get status() {
    // TODO: Implement this
    return this._status
  }

  /*
   * Set the failsafes for each channel
   */
  setFailsafes (failsafes) {
    this._ws.send(
      JSON.stringify(
        {
          failsafes: failsafes
        }
      )
    )
  }

  /*
   * Set the pulsewidth for each servo
   */
  setServos (servos) {
    this._ws.send(
      JSON.stringify(
        {
          servos: servos
        }
      )
    )
  }

  /*
   * Update settings
   */
  updateSettings (settings) {
    this._ws.send(
      JSON.stringify(
        {
          "settings" : settings
        }
      )
    )
  }

  /*
   * Reset default settings
   */
  resetSettings () {
    this._ws.send(
      JSON.stringify(
        {
          reset_settings : "sgnittes_teser"
        }
      )
    )
  }

  /*
   * Reboot
   */
  reboot () {
    this._ws.send(
      JSON.stringify(
        {
          reboot : "toober"
        }
      )
    )
  }

  /*
   * Query Battery
   */
  async getBatteryVoltage () {
    this._ws.send(
      JSON.stringify(
        {
          query : "battery"
        }
      )
    )
    // TODO: Await response and return
  }

  /*
   * Query Failsafes
   */
  async getFailsafes () {
    this._ws.send(
      JSON.stringify(
        {
          query : "failsafes"
        }
      )
    )
    // TODO: Await response and return
  }

  /*
   * Query Settings
   */
  async getSettings () {
    this._ws.send(
      JSON.stringify(
        {
          query : "settings"
        }
      )
    )
    // TODO: Await response and return
  }
}

