class Hexapod {
  constructor (options = {}) {
    this.reset()

    this._ticking = false
  }

  resetConstants() {
    this._trim = [ 60, -20, -40,
                   -70, 20, 0,
                   0, 20, 0,
                   0, 20, 0,
                   0, 20, 0,
                   90, -40, 0,
                   0 ]
    this._width = 100

    this._coxaLength = 15
    this._femurLength = 50
    this._tibiaLength = 71.589
    this._footRadius = 3.75
    this._bodyWidth = 100
    this._legSpacing = 62

    this._legPositions = [
      new Vector([this._bodyWidth / 2, this._legSpacing, 0]), new Vector([this._bodyWidth / 2, 0, 0]), new Vector([this._bodyWidth / 2, -this._legSpacing, 0]),
      new Vector([-this._bodyWidth / 2, -this._legSpacing, 0]), new Vector([-this._bodyWidth / 2, 0, 0]), new Vector([-this._bodyWidth / 2, this._legSpacing, 0])
    ]
    this._legDirections = [ new Vector([-1, 0, 0]), new Vector([-1, 0, 0]), new Vector([-1, 0, 0]),
                            new Vector([1, 0, 0]), new Vector([1, 0, 0]), new Vector([1, 0, 0]) ]

    this._defaultTargets = []

    for (var leg = 0; leg < 6; ++leg) {
      this._defaultTargets.push(Vector.add(Vector.add(this._legPositions[leg], Vector.mul(this._legDirections[leg], this._coxaLength + this._femurLength / 2)), new Vector([0, 0, -20])))
    }
  }

  reset () {
    this._smoothed = [ 1500, 1500, 1500,
                       1500, 1500, 1500,
                       1500, 1500, 1500,
                       1500, 1500, 1500,
                       1500, 1500, 1500,
                       1500, 1500, 1500,
                       1500 ]
    this._pulsewidths = [ 1500, 1500, 1500,
                          1500, 1500, 1500,
                          1500, 1500, 1500,
                          1500, 1500, 1500,
                          1500, 1500, 1500,
                          1500, 1500, 1500,
                          1500 ]
    this._enabledServos = [ 0, 0, 0,
                            0, 0, 0,
                            0, 0, 0,
                            0, 0, 0,
                            0, 0, 0,
                            0, 0, 0,
                            0 ]

    this._inputTranslation = new Vector([0, 0])
    this._inputRotation = new Vector([0, 0])
    this._inputStance = new Vector([0, 0])
    this._inputLegs = []

    for (var leg = 0; leg < 6; ++leg) {
      this._inputLegs.push(new Vector([0, 0]))
    }

    this._debug = [ {}, {}, {}, {}, {}, {}, ]

    this.resetConstants()
  }

  stopServos() {
    for (var i = 0; i < this._enabledServos.length; ++i) {
      this._enabledServos[i] = 0
    }
  }

  resetServos() {
    this.stopServos()

    //this.reset()

    let localThis = this

    let resetServosHandle = setInterval(function() {
      let anyEnabled = false

      // Enable the first non-enabled servo in the list
      for (var i = 0; i < localThis._enabledServos.length; ++i) {
        if (localThis._enabledServos[i] === 0) {
          localThis._enabledServos[i] = 1
          anyEnabled = true
          break;
        }
      }

      // If all servos are enabled clear the timer
      if (!anyEnabled) {
        clearInterval(resetServosHandle)
      }
    }, 100)
  }

  setTranslation (x, y) {
    this._inputTranslation = Vector.mul(new Vector([x, y]), 50)
  }

  setRotation (x, y) {
    this._inputRotation = new Vector([x, y])
  }

  setStance (x, y) {
    this._inputStance = Vector.mul(new Vector([x, y]), 50)
  }

  setLeg (leg, x, y) {
    if (leg >= 0 && leg < 6) {
      this._inputLegs[leg] = Vector.mul(new Vector([x, y]), 50)
    }
  }

  // Convert an angle in degrees to a pulse width
  // The servos swing about 55 degrees each way from centre (500 microsecond change in pulse width)
  convertAngle (angle) {
    return 1500 + ((angle / 55.0) * 250)
  }

  // Default orientation for each coxa is always the reference frame
  // Body transformations are inverted and applied to all targets
  solveLeg (leg, target) {
    // Calculate the distance from the coxa to the target
    const offset = Vector.sub(target, this._legPositions[leg])
    const distance = Vector.len(offset)

    // Calculate offset in the horizontal plane only
    const hOffset = new Vector([offset.x, offset.y])
    const hDistance = Vector.len(hOffset)

    // Calculate the angle from the coxa default orientation to the target
    // Since the legs are oriented horizontally we can get away with just ata2n
    const coxaAngle = Math.atan(offset.y / offset.x) * (180 / Math.PI)

    // Calculate the tibia angle by cosine law, add the tibia rest pose of 113 degrees
    const tibiaAngle = 113 - (Math.acos((this._tibiaLength * this._tibiaLength + this._femurLength * this._femurLength - distance * distance) / (2 * this._tibiaLength * this._femurLength)) * (180 / Math.PI))

    // Calculate the femur to target angle by cosine law, then subtract the angle to the target
    const targetAngle = Math.atan(offset.z / hDistance)
    const femurAngle = (Math.acos((this._femurLength * this._femurLength + distance * distance - this._tibiaLength * this._tibiaLength) / (2 * this._femurLength * distance)) - targetAngle) * (180 / Math.PI)

    return {
      coxaAngle,
      femurAngle,
      tibiaAngle
    }
  }

  tick () {
    if (this._ticking) {
      return
    }
    this._ticking = true

    this.resetConstants()

    for (var leg = 0; leg < 6; ++leg) {
      let inputLeg = this._inputLegs[leg]
      let target = Vector.add(this._defaultTargets[leg], this._inputTranslation)
      target = Vector.add(target, new Vector(0, 0, this._inputStance.y))

      target = Vector.add(target, Vector.add(new Vector([0, 0, inputLeg.y]), Vector.mul(this._legDirections[leg], inputLeg.x)))

      let angles = this.solveLeg(leg, target)
      let mirror = (leg > 2) ? -1 : 1

      this._debug[leg] = Object.assign(angles, { target })

      let legOffset = leg * 3

      this._pulsewidths[legOffset + 0] = this.convertAngle(angles.coxaAngle)
      this._pulsewidths[legOffset + 1] = this.convertAngle(angles.femurAngle * mirror)
      this._pulsewidths[legOffset + 2] = this.convertAngle(angles.tibiaAngle * mirror)

      for (var joint = legOffset; joint < legOffset + 3; ++joint) {
        this._smoothed[joint] = parseInt(0.95 * this._smoothed[joint] + 0.05 * this._pulsewidths[joint])
      }
    }

    /*
    // Direct control of the legs
    for (var i = 0; i < 6; ++i) {
      this._smoothed[i * 3 + 0] = 1500 + (this._inputTranslation.x * 12)
      this._smoothed[i * 3 + 1] = 1500 + (this._inputLegs[i].x * 12)
      this._smoothed[i * 3 + 2] = 1500 + (this._inputLegs[i].y * 12)
    }
    */

    this._ticking = false
  }

  get outputs () {
    const output = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]

    for (var i = 0; i < 19; ++i) {
      if (this._enabledServos[i]) {
        let value = this._smoothed[i]

        // Don't apply trim or clamp zeroes - they disable the servo
        if (value !== 0) {
          //value += this._trim[i]

          if (value < 1000) {
            value = 1000
          } else if (value > 2000) {
            value = 2000
          }
        }

        output[i] = value
      }
    }

    return output
  }
}

