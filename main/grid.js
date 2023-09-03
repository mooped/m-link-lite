class Application {
  constructor (parent, options = {}) {
    this._parent = parent
    this._options = options
  }

  _render () {
    this._parent.style.position = 'relative'
    this._parent.style.width = '100%'
    this._parent.style.height = '100%'
    if (this._options.width) {
      this._parent.style.gridColumn = 'span ' + this._options.width.toString()
    }
    if (this._options.height) {
      this._parent.style.gridRow = 'span ' + this._options.height.toString()
    }
    this._parent.style.border = '1px solid black'
  }

  _activateListeners () {

  }

  create() {
    this._render()
    this._activateListeners()
  }

  destroy() {
    this._parent.remove()
  }
}

class Joystick extends Application {
  constructor (parent, options = {}) {
    super(parent, options)

    this._edit = options.edit || false
  }

  _render () {
    super._render()

    if (!this._edit) {
      this._stick = nipplejs.create({
        zone: this._parent,
        mode: 'static',
        position: { left: '50%', top: '50%' },
        color: '#105905',
        size: 200,
      })
    }
  }

  _activateListeners () {
    super._activateListeners()
  }
}

class Button extends Application {
  constructor (parent, options = {}) {
    super(parent, options)
  }

  _render () {
    super._render()
  }

  _activateListeners () {
    super._activateListeners()
  }
}
