class MLink {
  /*
   * Construct an MLink object to talk to an M-Link Lite robot controller
   */
  constructor (uri, options = {}) {
    this.onmessage = undefined
    this.onopen = undefined
    this.onclose = undefined

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

    this._ws.onmessage = function (evt) {
      let obj = JSON.parse(evt.data)

      if (this.onmessage) {
        this.onmessage(obj)
      }
    }
    ws.onopen = function () {
      if (this.onopen) {
        this.onopen()
      }
    }
    ws.onclose = function (evt) {
      if (this.onclose) {
        this.onclose(evt.code, evt.reason)
      }
    }

    // Send initial settings query
    this.ws.send(
      JSON.stringify(
        {
          query: "settings"
        }
      )
    )
  }

  /*
   * Set the failsafes for each channel
   */
  setFailsafes (failsafes) {
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
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
    this.ws.send(
      JSON.stringify(
        {
          query : "settings"
        }
      )
    )
    // TODO: Await response and return
  }
}

