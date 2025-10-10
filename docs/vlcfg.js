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

export class Types {
  static TEXT = 't';
  static PASS = 'p';
  static IP_ADDR = 'd';
  static NUMBER = 'n';
  static CHECK = 'c';
};

/**
 * @param {Object} formJson 
 * @returns {VlcfgForm}
 */
export function makeFormJson(formJson) {
  return new VlcfgForm(formJson);
};

export function makeFromUrlHash(hash) {
  hash = decodeURIComponent(window.location.hash);

  if (hash.startsWith("#")) {
    hash = hash.substring(1);
  }

  if (!hash.startsWith("form:")) {
    throw new Error("Invalid hash format: '" + hash + "'");
  }
  const formJson = ShortenJSON.parse(hash.substring(5));

  replaceKey(formJson, 't', 'title');
  replaceKey(formJson, 'e', 'entries');
  for (const entry of formJson.entries) {
    replaceKey(entry, 'k', 'key');
    replaceKey(entry, 't', 'type');
    replaceKey(entry, 'l', 'label');
    replaceKey(entry, 'v', 'value');
    replaceKey(entry, 'p', 'placeholder');
  }

  return new VlcfgForm(formJson);
}

function replaceKey(obj, shortKey, fullKey) {
  if (shortKey in obj) {
    obj[fullKey] = obj[shortKey];
    delete obj[shortKey];
  }
}

export class VlcfgForm {
  header = document.createElement("h1");
  form = document.createElement("form");
  instruction = makeSpan(getLabel("place_sensor_on_circle"));
  submitButton = makeButton(getLabel("submit"));
  cancelButton = makeButton(getLabel("cancel"));
  progress = document.createElement("progress");
  lamp = makeDiv([], "vlcfg_lamp");
  lampNote = makeSpan(getLabel("circle_above_flashes"), "vlcfg_note");
  container = makeDiv([
    this.header,
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
    this.header,
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
    if (formJson.title) {
      this.header.textContent = formJson.title;
    }

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
    this.lamp.style.background = "#888";
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
  container = makeParagraph();

  /**
   * @param {Object} entryJson 
   */
  constructor(entryJson) {
    if (!entryJson.key || !entryJson.label || !entryJson.type) {
      throw new Error("Invalid form entry");
    }
    this.key = entryJson.key;
    this.type = entryJson.type;
    switch (entryJson.type) {
      case Types.TEXT:
        this.input.type = "text";
        break;
      case Types.PASS:
        this.input.type = "password";
        this.input.autocomplete = "off";
        break;
      case Types.IP_ADDR:
        this.input.type = "text";
        break;
      case Types.NUMBER:
        this.input.type = "number";
        break;
      case Types.CHECK:
        this.input.type = "checkbox";
        break;
      default:
        throw new Error("Invalid form entry type");
    }

    if (entryJson.type == Types.CHECK) {
      this.input.checked = entryJson.value ? true : false;
      this.label.appendChild(this.input);
      this.container.appendChild(this.label);
    }
    else {
      this.input.value = entryJson.value || "";
      this.container.appendChild(this.label);
      this.container.appendChild(makeBr());
      this.container.appendChild(this.input);
      this.input.placeholder = entryJson.placeholder || "";
    }

    this.label.appendChild(document.createTextNode(entryJson.label));
  }

  pushToPayload(payload) {
    if (!this.input.value) {
      return false;
    }

    pushTextString(payload, this.key);
    switch (this.type) {
      case Types.TEXT:
      case Types.PASS:
        pushTextString(payload, this.input.value);
        break;

      case Types.IP_ADDR: {
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
      } break;

      case Types.NUMBER: {
        const valStr = this.input.value;
        const val = Number(valStr);
        if (!Number.isInteger(val)) {
          throw new Error("Floating point number is not supported");
        }
        pushInt(payload, BigInt(val));
      } break;

      case Types.CHECK:
        pushBoolean(payload, this.input.checked);
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

function pushInt(payload, value) {
  if (value >= 0n) {
    pushMajorType(payload, 0x00, value);
  }
  else {
    pushMajorType(payload, 0x20, -1n - value);
  }
}

function pushBoolean(payload, value) {
  pushMajorType(payload, 0xE0, value ? 21 : 20);
}

function pushMajorType(payload, majorType, param) {
  param = BigInt(param);
  if (param < 0) {
    throw new Error("Parameter out of range");
  }
  if (param < 24) {
    payload.push(majorType + Number(param));
  }
  else if (param < 0x10000000000000000n) {
    let buff = [];
    let tmp = param;
    do {
      buff.push(Number(tmp & 0xFFn));
      tmp = tmp >> 8n;
    } while (tmp > 0);

    if (buff.length > 4) {
      while (buff.length < 8) buff.push(0);
      payload.push(majorType + 27);
    }
    else if (buff.length > 2) {
      while (buff.length < 4) buff.push(0);
      payload.push(majorType + 26);
    }
    else if (buff.length > 1) {
      while (buff.length < 2) buff.push(0);
      payload.push(majorType + 25);
    }
    else {
      payload.push(majorType + 24);
    }

    for (let i = buff.length - 1; i >= 0; i--) {
      payload.push(buff[i]);
    }
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

class ShortenJSON {
  /**
   * @param {string} str 
   * @returns {any}
   */
  static parse(str) {
    const reader = new StringReader(str);
    return ShortenJSON.parseValue(reader);
  }

  /** 
   * @param {StringReader} reader 
   * @returns {any}
   */
  static parseValue(reader) {
    let ret = null;
    if (reader.readIfMatch('{')) {
      return ShortenJSON.parseObjectFollowing(reader);
    }
    else if (reader.readIfMatch('[')) {
      return ShortenJSON.parseArrayFollowing(reader);
    }
    else if ((ret = reader.readIfNumber()) !== null) {
      return ret;
    }
    else if ((ret = reader.readIfBoolean()) !== null) {
      return ret;
    }
    else if ((ret = reader.readIfString()) !== null) {
      return ret;
    }
  }

  /** 
   * @param {StringReader} reader 
   * @returns {Object}
   */
  static parseObjectFollowing(reader) {
    const obj = {};
    while (!reader.readIfMatch('}')) {
      const key = reader.readString();
      reader.expect(':');
      const value = ShortenJSON.parseValue(reader);
      obj[key] = value;
      if (reader.readIfMatch('}')) break;
      reader.expect(',');
    }
    return obj;
  }

  /** 
   * @param {StringReader} reader 
   * @returns {Array}
   */
  static parseArrayFollowing(reader) {
    const arr = [];
    while (!reader.readIfMatch(']')) {
      arr.push(ShortenJSON.parseValue(reader));
      if (reader.readIfMatch(']')) break;
      reader.expect(',');
    }
    return arr;
  }
}

class StringReader {
  static RE_CHARS_TO_BE_QUOTED = /[\{\}\[\]:,\\]/;
  static KEYWORDS = ['true', 'false', 'null'];

  constructor(str) {
    this.str = str;
    this.pos = 0;
  }

  peek(length = 1) {
    if (this.pos + length > this.str.length) {
      return null;
    }
    return this.str.substring(this.pos, this.pos + length);
  }

  read(length = 1) {
    const s = this.peek(length);
    if (s === null) {
      throw new Error("Unexpected end of string");
    }
    this.pos += length;
    return s;
  }

  back(length = 1) {
    this.pos -= length;
    if (this.pos < 0) {
      throw new Error("Position out of range");
    }
  }

  readIfMatch(s) {
    const len = s.length;
    if (this.peek(len) === s) {
      this.pos += len;
      return true;
    }
    return false;
  }

  readDecimalString() {
    let numStr = "";
    while (true) {
      const ch = this.peek();
      if (ch !== null && /[0-9]/.test(ch)) {
        numStr += this.read();
      }
      else {
        break;
      }
    }
    if (numStr.length === 0) {
      throw new Error("Decimal number expected");
    }
    return numStr;
  }

  readIfNumber() {
    let numStr = "";
    if (this.readIfMatch('-')) {
      numStr += '-';
    }
    else if (this.readIfMatch('+')) {
      numStr += '+';
    }
    else {
      const first = this.peek();
      if (first === null || !/[0-9]/.test(first)) {
        return null;
      }
    }
    numStr += this.readDecimalString();

    if (this.readIfMatch('.')) {
      numStr += '.';
      numStr += this.readDecimalString();
    }
    if (this.readIfMatch('e') || this.readIfMatch('E')) {
      numStr += 'e';
      if (this.readIfMatch('+')) {
        numStr += '+';
      }
      else if (this.readIfMatch('-')) {
        numStr += '-';
      }
      numStr += this.readDecimalString();
    }
    return Number(numStr);
  }

  readIdentifier() {
    const id = this.readIfIdentifier();
    if (id === null) {
      throw new Error("Identifier expected");
    }
    return id;
  }

  expect(s) {
    if (this.readIfMatch(s)) {
      return;
    }
    throw new Error(`Keyword "${s}" expected`);
  }

  readStringChar(quotation) {
    const ch = this.peek();
    if (quotation && ch === quotation) {
      return null;
    }
    else if (!quotation && StringReader.RE_CHARS_TO_BE_QUOTED.test(ch)) {
      return null;
    }
    else if (ch === '\\') {
      this.read();
      const esc = this.read();
      switch (esc) {
        case '"': str += '"'; break;
        case '\\': str += '\\'; break;
        case '/': str += '/'; break;
        case 'b': str += '\b'; break;
        case 'f': str += '\f'; break;
        case 'n': str += '\n'; break;
        case 'r': str += '\r'; break;
        case 't': str += '\t'; break;
        case 'u':
          let hex = "";
          for (let i = 0; i < 4; i++) {
            const h = this.read();
            if (!/[0-9a-fA-F]/.test(h)) {
              throw new Error("Invalid Unicode escape");
            }
            hex += h;
          }
          return String.fromCharCode(parseInt(hex, 16));
        default:
          throw new Error("Invalid escape character");
      }
    }
    else {
      return this.read();
    }
  }

  readIfString() {
    const next = this.peek();

    if (next === '"' || next === "'") {
      const quotation = this.read();
      let str = "";
      while (true) {
        const ch = this.readStringChar(quotation);
        if (ch === null) {
          break;
        }
        str += ch;
      }
      this.expect(quotation);
      return str;
    }

    if (next && !StringReader.RE_CHARS_TO_BE_QUOTED.test(next) || !/[0-9]/.test(next)) {
      let str = "";
      while (true) {
        const ch = this.readStringChar(null);
        if (ch === null) {
          break;
        }
        str += ch;
      }

      if (StringReader.KEYWORDS.includes(str)) {
        this.back(str.length);
        return null;
      }

      return str;
    }

    return null;
  }

  readString() {
    const str = this.readIfString();
    if (str === null) {
      throw new Error("String expected");
    }
    return str;
  }

  readIfBoolean() {
    if (this.readIfMatch('true')) {
      return true;
    }
    if (this.readIfMatch('false')) {
      return false;
    }
    return null;
  }

  eof() {
    return this.pos >= this.str.length;
  }

}