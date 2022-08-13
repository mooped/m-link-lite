class Counter {
  /*
   * Construct a Counter object to control a countdown clock
   */
  constructor (initialTime, options = {}) {
    // Default event handlers to undefined
    this.oncomplete = undefined
    this.onreset = undefined
    this.events = {}

    // Set event handlers from options
    if (options.oncomplete) {
      this.oncomplete = options.oncomplete
    }
    if (options.onreset) {
      this.onreset = options.onreset
    }
    if (options.events) {
      this.events = options.events
    }

    // Store initial time and set remaining time
    this._initialTime = initialTime
    this._remainingTime = initialTime
  }

  static zeroPad (value) {
    const str = value.toString()
    if (str.length == 0) {
      return '00'
    } else if (str.length == 1) {
      return '0' + str
    } else {
      return str
    }
  }

  /*
   * Reset the counter
   */
  reset (newTime) {
    if (newTime) {
      this._initialTime = newTime
    }
    if (this.onreset) {
      this.onreset(this)
    }
    this._remainingTime = this._initialTime
  }

  /*
   * Update the counter with a delta time
   */
  update (deltaTime) {
    if (this._remainingTime > 0) {
      this._remainingTime = this._remainingTime - deltaTime
      if (this._remainingTime <= 0) {
        this._remainingTime = 0
        if (this.oncomplete) {
          this.oncomplete(this)
        }
      }
    }
    return this.remainingTime
  }

  /*
   * Get initial time
   */
  get initialTime () {
    return this._initialTime
  }

  /*
   * Get current time
   */
  get remainingTime () {
    return this._remainingTime
  }

  /*
   * Get Minutes
   */
  get minutes () {
    return Math.floor(this._remainingTime / 60000)
  }

  /*
   * Get Seconds
   */
  get seconds () {
    return Math.floor((this._remainingTime / 1000) % 60)
  }

  /*
   * Get Hundredths
   */
  get hundredths () {
    return Math.floor((this._remainingTime % 1000) / 10)
  }

  /*
   * Get initial Minutes
   */
  get initialMinutes () {
    return Math.floor(this._initialTime / 60000)
  }

  /*
   * Get initial Seconds
   */
  get initialSeconds () {
    return Math.floor((this._initialTime / 1000) % 60)
  }

  /*
   * Get initial Hundredths
   */
  get initialHundredths () {
    return Math.floor((this._initialTime % 1000) / 10)
  }

  /*
   * Get formatted initial time
   */
  get formattedInitialTime () {
    const minutes = Counter.zeroPad(this.initialMinutes)
    const seconds = Counter.zeroPad(this.initialSeconds)
    const hundredths = Counter.zeroPad(this.initialHundredths)
    return `${minutes}:${seconds}.${hundredths}`
  }

  /*
   * Get formatted current time
   */
  get formattedRemainingTime () {
    const minutes = Counter.zeroPad(this.minutes)
    const seconds = Counter.zeroPad(this.seconds)
    const hundredths = Counter.zeroPad(this.hundredths)
    return `${minutes}:${seconds}.${hundredths}`
  }
}

