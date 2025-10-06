const vlcfg = (function () {
  'use strict';

  const RE_IPV4_ADDR = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;

  const SYMBOL_BITS = 5;
  const SYMBOL_CONTROL = 0b01010;
  const SYMBOL_SYNC = 0b10001;
  const SYMBOL_SOF = 0b00011;
  const SYMBOL_EOF = 0b00111;
  const SYMBOL_TABLE = [
    0b00101, 0b00110, 0b01001, 0b01011,
    0b01100, 0b01101, 0b01110, 0b10010,
    0b10011, 0b10100, 0b10101, 0b10110,
    0b11000, 0b11001, 0b11010, 0b11100
  ];

  const TYPE_CHAR_TEXT = 't';
  const TYPE_CHAR_PASSWORD = 'p';
  const TYPE_CHAR_IP_ADDRESS = 'd';

  class Vlcfg {

    /**
     * @param {Object} formJson 
     * @returns {VlcfgForm}
     */
    makeForm(formJson) {
      return new VlcfgForm(formJson);
    }

  }

  class VlcfgForm {
    form = document.createElement("form");
    instruction = makeSpan(getLabel("place_sensor_on_circle"));
    submitButton = makeButton(getLabel("submit"));
    cancelButton = makeButton(getLabel("cancel"));
    progress = document.createElement("progress");
    lamp = makeDiv([], "vlcfg_lamp");
    lampNote = makeSpan(getLabel("circle_above_flashes"), "vlcfg_note");
    container = makeDiv([
      this.form,
      makeParagraph(this.instruction, null, true),
      makeParagraph([this.submitButton, this.cancelButton,], null, true),
      makeParagraph([this.progress], null, true),
      makeParagraph([
        this.lamp,
        this.lampNote,
      ], "vlcfg_lamp_wrap", true),
    ], "vlcfg_container");

    elementsToBeHidden = [
      this.form,
      this.instruction,
      this.lampNote,
    ];

    entries = [];

    sendingSequence = null;
    nextBitPos = 0;
    nextBitTime = 0;
    wakeLock = null;

    /**
     * @param {Object} formJson 
     */
    constructor(formJson) {

      for (const entryJson of formJson.entries) {
        const entry = new FormEntry(entryJson);
        this.entries.push(entry);
        this.form.appendChild(entry.container);
      }

      this.progress.max = 100;
      this.progress.value = 0;
      this.progress.style.width = "100%";

      this.submitButton.type = "submit";
      this.cancelButton.disabled = true;

      this.submitButton.addEventListener("click", (e) => {
        e.preventDefault();
        this.send();
      });
      this.cancelButton.addEventListener("click", (e) => {
        e.preventDefault();
        this.cancel();
      });
    }

    async send() {
      let payload = [];
      let numEntries = 0;
      payload.push(0xA0);
      for (const entry of this.entries) {
        if (entry.pushToPayload(payload)) {
          numEntries++;
        }
      }
      payload[0] |= numEntries;
      let crc = crc32(payload);
      payload.push(Math.floor(crc / 0x1000000) & 0xff);
      payload.push(Math.floor(crc / 0x10000) & 0xff);
      payload.push(Math.floor(crc / 0x100) & 0xff);
      payload.push(Math.floor(crc / 0x1) & 0xff);

      let hexStr = "";
      for (const byte of payload) {
        hexStr += byte.toString(16).padStart(2, '0').toUpperCase();
      }
      console.log("Payload: " + hexStr);

      const seq = new LightSequence();
      for (let i = 0; i < 7; i++) {
        seq.pushSymbol(SYMBOL_CONTROL);
        seq.pushSymbol(SYMBOL_SYNC);
      }
      seq.pushSymbol(SYMBOL_CONTROL);
      seq.pushSymbol(SYMBOL_SOF);
      for (const byte of payload) {
        seq.pushByte(byte);
      }
      seq.pushSymbol(SYMBOL_CONTROL);
      seq.pushSymbol(SYMBOL_EOF);
      seq.pushSymbol(SYMBOL_CONTROL);
      seq.pushSymbol(SYMBOL_SYNC);

      this.submitButton.disabled = true;
      this.cancelButton.disabled = false;
      for (const elm of this.elementsToBeHidden) {
        //elm.style.visibility = "hidden";
        fade(elm, false, 500);
      }

      this.sendingSequence = seq;
      this.nextBitPos = 0;
      this.nextBitTime = performance.now() + 100;
      this.animate(performance.now());

      try {
        if ('wakeLock' in navigator) {
          this.wakeLock = await navigator.wakeLock.request('screen');
        }
      }
      catch (err) {
        console.error("Wake Lock error: ", err);
      }
    }

    animate(now) {
      let stop = false;
      const seq = this.sendingSequence;

      if (!seq) {
        stop = false;
      }
      else if (now >= this.nextBitTime) {
        const len = seq.commands.length;
        const cmd = seq.commands[this.nextBitPos];
        this.lamp.style.background = cmd.lampValue ? "white" : "black";
        this.progress.value = (this.nextBitPos / len) * 100;
        this.nextBitTime += 100;
        this.nextBitPos++;
        if (this.nextBitPos >= len) {
          stop = true;
        }
      }

      if (stop) {
        this.reset();
        this.progress.value = 100;
      } else {
        requestAnimationFrame(now => this.animate(now));
      }
    }

    cancel() {
      this.progress.value = 0;
      this.reset();
    }

    reset() {
      this.lamp.style.background = "#444";
      for (const elm of this.elementsToBeHidden) {
        //elm.style.visibility = "visible";
        fade(elm, true, 500);
      }
      this.submitButton.disabled = false;
      this.cancelButton.disabled = true;
      this.sendingSequence = null;

      if (this.wakeLock) {
        this.wakeLock.release().then(() => {
          this.wakeLock = null;
        });
      }
    }

    getElement() {
      return this.container;
    }

    /**
     * @param {string} parentSelector 
     * @throws {Error}
     */
    appendTo(parentSelector) {
      const parent = document.querySelector(parentSelector);
      if (parent) {
        parent.appendChild(this.container);
      }
      else {
        throw new Error("Parent element not found: " + parentSelector);
      }
    }
  }

  class LightCommand {
    /** @type {number} */
    lampValue;
    /** @type {number} */
    delayTime;

    /**
     * @param {number} lampValue 
     * @param {number} delayTime 
     */
    constructor(lampValue, delayTime) {
      this.lampValue = lampValue;
      this.delayTime = delayTime;
    }
  }

  class LightSequence {
    /** @type {Array<LightCommand>} */
    commands = [];

    pushSymbol(symbol) {
      for (let i = SYMBOL_BITS - 1; i >= 0; i--) {
        const bit = (symbol >> i) & 1;
        const cmd = new LightCommand(bit ? 1 : 0, 1);
        this.commands.push(cmd);
      }
    }

    pushByte(byte) {
      if (byte < 0 || byte > 255 || !Number.isInteger(byte)) {
        throw new Error("Byte out of range");
      }
      this.pushSymbol(SYMBOL_TABLE[(byte >> 4) & 0x0F]);
      this.pushSymbol(SYMBOL_TABLE[byte & 0x0F]);
    }
  }

  class FormEntry {
    label = document.createElement("label");
    input = document.createElement("input");
    container = makeParagraph([this.label, makeBr(), this.input]);

    /**
     * @param {Object} entryJson 
     */
    constructor(entryJson) {
      if (!entryJson.key || !entryJson.label || !entryJson.type) {
        throw new Error("Invalid form entry");
      }
      this.key = entryJson.key;
      this.type = entryJson.type;
      this.label.textContent = entryJson.label;
      switch (entryJson.type) {
        case TYPE_CHAR_TEXT:
          this.input.type = "text";
          break;
        case TYPE_CHAR_PASSWORD:
          this.input.type = "password";
          this.input.autocomplete = "off";
          break;
        case TYPE_CHAR_IP_ADDRESS:
          this.input.type = "text";
          break;
        default:
          throw new Error("Invalid form entry type");
      }
      this.input.placeholder = entryJson.placeholder || "";
    }

    pushToPayload(payload) {
      if (!this.input.value) {
        return false;
      }

      pushTextString(payload, this.key);
      switch (this.type) {
        case TYPE_CHAR_TEXT:
        case TYPE_CHAR_PASSWORD:
          pushTextString(payload, this.input.value);
          break;
        case TYPE_CHAR_IP_ADDRESS: {
          const mV4 = RE_IPV4_ADDR.exec(this.input.value);
          if (mV4) {
            const bytes = [
              parseInt(mV4[1], 10),
              parseInt(mV4[2], 10),
              parseInt(mV4[3], 10),
              parseInt(mV4[4], 10),
            ];
            for (let i = 0; i < 4; i++) {
              if (bytes[i] < 0 || bytes[i] > 255) {
                throw new Error("Invalid IP address format");
              }
            }
            pushByteStr(payload, bytes);
          }
          else {
            throw new Error("Invalid IP address format");
          }
        }
          break;
        default:
          throw new Error("Invalid form entry type");
      }
      return true;
    }
  }

  function pushTextString(payload, str) {
    const len = str.length;
    pushMajorType(payload, 0x60, len);
    for (let i = 0; i < len; i++) {
      payload.push(str.charCodeAt(i) & 0xFF);
    }
  }

  function pushByteStr(payload, byteArray) {
    const len = byteArray.length;
    pushMajorType(payload, 0x40, len);
    for (let i = 0; i < len; i++) {
      if (byteArray[i] < 0 || byteArray[i] > 255 || !Number.isInteger(byteArray[i])) {
        throw new Error("Byte out of range");
      }
      payload.push(byteArray[i]);
    }
  }

  function pushUnsignedInt(payload, value) {
    pushMajorType(payload, 0x00, value);
  }

  function pushMajorType(payload, majorType, param) {
    if (param < 24) {
      payload.push(majorType + param);
    }
    else if (param < 0x100) {
      payload.push(majorType + 24);
      payload.push(param);
    }
    else if (param < 0x10000) {
      payload.push(majorType + 25);
      payload.push((param >> 8) & 0xFF);
      payload.push(param & 0xFF);
    }
    else if (param < 0x100000000) {
      payload.push(majorType + 26);
      payload.push(Math.floor(param / 0x1000000) & 0xFF);
      payload.push(Math.floor(param / 0x10000) & 0xFF);
      payload.push(Math.floor(param / 0x100) & 0xFF);
      payload.push(Math.floor(param / 0x1) & 0xFF);
    }
    else {
      throw new Error("Number too large");
    }
  }

  function crc32(byteArray) {
    let crc = 0xffffffff;
    for (let i = 0; i < byteArray.length; i++) {
      let byte = byteArray[i];
      crc ^= byte;
      for (let j = 0; j < 8; j++) {
        let mask = -(crc & 1);
        crc = (crc >>> 1) ^ (0xedb88320 & mask);
      }
    }
    return ~crc >>> 0;
  }

  /**
   *
   * @param {string | Node | Array<string | Node> | null} children
   * @param {boolean} center
   * @returns {HTMLParagraphElement}
   */
  function makeParagraph(children, className = null, center = false) {
    const elm = document.createElement('p');
    toElementArray(children).forEach(child => elm.appendChild(child));
    if (className) {
      elm.classList.add(className);
    }
    if (center) {
      elm.style.textAlign = "center";
    }
    return elm;
  }

  /**
   * @param {string | Node | Array<string | Node> | null} children
   * @param {string | null} className
   * @returns {HTMLDivElement}
   */
  function makeDiv(children, className = null) {
    const elm = document.createElement('div');
    toElementArray(children).forEach(child => elm.appendChild(child));
    if (className) {
      elm.classList.add(className);
    }
    return elm;
  }

  /**
   * @param {string} text
   * @param {string} className
   * @returns {HTMLSpanElement}
   */
  function makeSpan(text, className = null) {
    const elm = document.createElement("span");
    elm.textContent = text;
    if (className) {
      elm.classList.add(className);
    }
    return elm;
  }

  /**
   * @param {string} label 
   * @returns {HTMLButtonElement}
   */
  function makeButton(label) {
    const elm = document.createElement("button");
    elm.textContent = label;
    return elm;
  }

  /**
   * @returns {HTMLBRElement}
   */
  function makeBr() {
    const elm = document.createElement("br");
    return elm;
  }

  /**
   * @param {string | Node | Array<string | Node> | null} children 
   * @returns {Array<Node>}
   */
  function toElementArray(children) {
    if (children == null) {
      return [];
    }
    if (!Array.isArray(children)) {
      children = [children];
    }
    for (let i = 0; i < children.length; i++) {
      if (typeof children[i] === 'string') {
        children[i] = document.createTextNode(children[i]);
      } else if (children[i] instanceof Node) {
        // Do nothing
      } else {
        throw new Error('Invalid child element');
      }
    }
    return children;
  }

  function fade(element, fadeIn = true, duration = 500) {
    element.style.opacity = fadeIn ? 0 : 1;
    if (fadeIn) {
      element.style.visibility = "visible";
    }

    let start = null;

    function step(now) {
      if (!start) start = now;
      const p = now - start;
      let opacity = fadeIn ? (p / duration) : (1 - (p / duration));
      element.style.opacity = Math.max(0, Math.min(1, opacity));
      if (p < duration) {
        window.requestAnimationFrame(step);
      }
      else {
        if (!fadeIn) {
          element.style.visibility = "hidden";
        }
      }
    }
    window.requestAnimationFrame(step);
  }

  const LABELS = {
    submit: {
      'en': 'Submit',
      'ja': '送信',
    },
    cancel: {
      'en': 'Cancel',
      'ja': 'キャンセル',
    },
    place_sensor_on_circle: {
      'en': 'Place the sensor on the circle, and press Submit.',
      'ja': 'センサーを円の上に置いて、送信ボタンを押してください。',
    },
    circle_above_flashes: {
      'en': 'The circle above will flash.',
      'ja': 'この円が点滅します',
    },
  };

  /**
   * @param {string} key 
   * @returns {string}
   */
  function getLabel(key) {
    const lang = navigator.language.startsWith("ja") ? "ja" : "en";
    return LABELS[key][lang];
  }

  return new Vlcfg();
})();

