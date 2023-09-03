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
    if (this._options.background) {
      this._parent.style.backgroundColor = this._options.background
    }
  }

  _activateListeners () {

  }

  create () {
    this._render()
    this._activateListeners()
  }

  destroy () {
    this._parent.remove()
  }
}

class Joystick extends Application {
  constructor (parent, options = {}) {
    super(parent, options)
  }

  _render () {
    super._render()

    this._stick = nipplejs.create({
      zone: this._parent,
      mode: 'static',
      position: { left: '50%', top: '50%' },
      color: this._options.foreground || '#105905',
      size: 200,
    })
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

    this._button = document.createElement('div')
    this._button.classList.add('button')
    this._state = 0
    if (this._options.foreground) {
      this._button.style.backgroundColor = this._options.foreground
    }
    this._parent.appendChild(this._button)
  }

  _activateListeners () {
    super._activateListeners()

    this._button.onmousedown = this._mouseDown.bind(this)
    this._button.onmouseup = this._mouseUp.bind(this)
    this._button.onmouseout = this._mouseUp.bind(this)
  }

  _mouseDown () {
    console.log("Down")
    this._button.style.backgroundColor = this._options.background
    this._state = 1
  }

  _mouseUp () {
    console.log("Up")
    this._button.style.backgroundColor = this._options.foreground
    this._state = 0
  }

  destroy () {
    super.destroy()

    this._button = null
  }
}
