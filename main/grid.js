class Application {
  constructor (parent, options = {}) {
    this._parent = parent
    this._options = options
  }

  get width () {
    return parseInt(this._options.width || 1)
  }

  get height () {
    return parseInt(this._options.height || 1)
  }

  get background () {
    return this._options.background || '#bbbbbb'
  }

  get foreground () {
    return this._options.foreground || '#105905'
  }

  get label () {
    return this._options.label || ''
  }

  get channels () {
    return {}
  }

  _render () {
    this._parent.style.position = 'relative'
    this._parent.style.width = '100%'
    this._parent.style.height = '100%'
    this._parent.style.gridColumn = 'span ' + this.width.toString()
    this._parent.style.gridRow = 'span ' + this.height.toString()
    this._parent.style.backgroundColor = this.background
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

    this._state = {
      x: 0.0,
      y: 0.0,
    }
  }

  get size () {
    return Math.min(this._parent.clientWidth, this._parent.clientHeight) / 1.5
  }

  get ca () {
    return parseInt(this._options.ca || 0)
  }

  get cb () {
    return parseInt(this._options.cb || 0)
  }

  get x () {
    return this._state.x || 0.0
  }

  get y () {
    return this._state.y || 0.0
  }

  get channels () {
    const data = {}
    data[this.ca] = this.x
    data[this.cb] = this.y
    return data
  }

  _render () {
    super._render()

    this._stick = nipplejs.create({
      zone: this._parent,
      mode: 'static',
      position: { left: '50%', top: '50%' },
      color: this.foreground,
      size: this.size,
    })
  }

  _activateListeners () {
    super._activateListeners()

    this._stick.on('move', this._move.bind(this))
    this._stick.on('end', this._end.bind(this))
  }

  destroy () {
    this._stick.destroy()
    super.destroy()
  }

  _move (evt, data) {
    this._state.x = data.vector.x
    this._state.y = data.vector.y
  }

  _end (evt) {
    this._state.x = 0.0
    this._state.y = 0.0
  }
}

class Button extends Application {
  constructor (parent, options = {}) {
    super(parent, options)
  }

  get idle () {
    return parseFloat(this._options.idle || -1)
  }

  get pressed () {
    return parseFloat(this._options.pressed || -1)
  }

  get ca () {
    return parseInt(this._options.ca || 0)
  }

  get state () {
    return this._state
  }

  get channels () {
    const data = {}
    data[this.ca] = this.state
    return data
  }

  _render () {
    super._render()

    this._button = document.createElement('div')
    this._button.classList.add('button')
    this._button.style.display = 'grid'
    this._button.style.border = '1px solid ' + this.foreground
    this._label = document.createElement('span')
    this._label.style.alignSelf = 'center'
    this._label.style.justifySelf = 'center'
    this._label.style.textAlign = 'center'
    this._label.textContent = this.label
    this._label.style.color = this.background
    this._state = this.idle
    this._button.style.backgroundColor = this.foreground
    this._parent.appendChild(this._button)
    this._button.appendChild(this._label)
  }

  _activateListeners () {
    super._activateListeners()

    this._button.onmousedown = this._mouseDown.bind(this)
    this._button.onmouseup = this._mouseUp.bind(this)
    this._button.onmouseout = this._mouseUp.bind(this)
    this._button.onpointerdown = this._mouseDown.bind(this)
    this._button.onpointerup = this._mouseUp.bind(this)
  }

  _mouseDown () {
    this._button.style.backgroundColor = this.background
    this._label.style.color = this.foreground
    this._state = this.pressed
  }

  _mouseUp () {
    this._button.style.backgroundColor = this.foreground
    this._label.style.color = this.background
    this._state = this.idle
  }
}

class Padding extends Application {
  constructor (parent, options = {}) {
    super(parent, options)
  }

  _render () {
    super._render()

    this._parent.style.display = 'grid'
    this._label = document.createElement('span')
    this._label.style.alignSelf = 'center'
    this._label.style.justifySelf = 'center'
    this._label.style.textAlign = 'center'
    this._label.textContent = this.label
    this._label.style.color = this.foreground
    this._parent.appendChild(this._label)
  }
}
