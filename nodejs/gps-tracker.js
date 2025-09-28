// CC0 1.0 Universal (CC0 1.0)
// Public Domain Dedication
// https://github.com/nmrr

const fs = require('node:fs');
const dgram = require('node:dgram');
const server = dgram.createSocket('udp4');
const {
  scrypt,
  randomFill,
  createDecipheriv,
  createHmac,
} = require('node:crypto');
const express = require('express')
const app = express()

// To generate keys: openssl rand -hex 32 | tee >(sed 's/../0x&, /g' | sed 's/, $/};/;s/^/{/' | sed '1s/^/\r\n/')
const AESKey = Buffer.from('YOUR AES256 KEY', 'hex')
const HMACKey = Buffer.from('YOUR HMAC SECRET', 'hex')

global.LONMap = "0"
global.LATMap = "0"
global.SPEEDMap = "0"
global.ALTMap = "0"
global.DATEMap = ""

server.on('error', (err) => {
  console.error(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (receive, rinfo) => {

  if (receive.length > 16+32)
  {
    IV = receive.slice(0,16)
    MESSAGE = receive.slice(16,receive.length-32)
    HMAC = receive.slice(receive.length-32,receive.length)

    console.log(IV.toString('hex'))

    if (createHmac('sha256', HMACKey).update(MESSAGE).digest('hex') == HMAC.toString('hex'))
    {
      console.log(rinfo.address+":"+rinfo.port)
      decrypted = createDecipheriv('aes-256-ctr', AESKey, IV).update(MESSAGE)
      console.log(receive)
      console.log(decrypted)

      if (decrypted.length == 24)
      {
        const timestampNow = Math.floor(Date.now()/1000)

        TIMESTAMP = decrypted.slice(0,8)
        LAT = decrypted.slice(8,12)
        LON = decrypted.slice(12,16)
        SPEED = decrypted.slice(16,20)
        ALT = decrypted.slice(20,24)

        const timestampMessage = Number(TIMESTAMP.readBigInt64LE())
        const date = new Date(timestampMessage * 1000)
        const datenow = new Date(Date.now())

        console.log("NOW : " + datenow.toString())
        console.log(timestampMessage + " - " + date.toString())
        console.log(LAT.readFloatLE().toFixed(6) + ", " + LON.readFloatLE().toFixed(6) + ", " + SPEED.readFloatLE().toFixed(1) + ", " + ALT.readFloatLE().toFixed(1))

        if (Math.abs(timestampNow - timestampMessage) <= 30)
        {
          console.log("Drift OK: " + Math.abs(timestampNow - timestampMessage))

          global.LATMap = LAT.readFloatLE().toFixed(6).toString()
          global.LONMap = LON.readFloatLE().toFixed(6).toString()
          global.SPEEDMap = SPEED.readFloatLE().toFixed(1).toString()
          global.ALTMap = ALT.readFloatLE().toFixed(1).toString()

          const formattedDate = new Intl.DateTimeFormat('fr-FR', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: 'numeric',
            second: 'numeric',
            timeZoneName: 'short'
          }).format(date);


          global.DATEMap = formattedDate

          const content = timestampNow.toString() + "," + timestampMessage.toString() + "," + LAT.readFloatLE().toFixed(6).toString() + "," + LON.readFloatLE().toFixed(6).toString() + "," + SPEED.readFloatLE().toFixed(1).toString() + "," + ALT.readFloatLE().toFixed(1).toString()+ "\n"

          fs.appendFile('./gps.log', content, err => {
            if (err) {
              console.error(err);
            }
          });
        }
        else
        {
          console.log("To much drift: " + Math.abs(timestampNow - timestampMessage))
        }
      }
    }
  }

});

server.on('listening', () => {
  const address = server.address();
  console.log(`UDP GPS tracker is listening on ${address.address}:${address.port}`);
});

server.bind(55100);

/////////////////////////////////////////////


app.get('/', (req, res) => {

  HTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <meta http-equiv="refresh" content="75">
  <title>Hello : 🛰️🌏📍</title>
  <link
    rel="stylesheet"
    href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"
    crossorigin=""
  />
  <style>
    body, html {
      margin: 0;
      height: 100%;
    }
    #map {
      height: 100%;
      width: 100%;
    }
    .overlay-box {
      position: absolute;
      top: 0px;
      left: 50%;
      transform: translateX(-50%);
      background-color: rgba(255, 255, 255, 0.65); /* Light semi-transparent */
      padding: 10px 10px;
      border-radius: 12px;
      font-family: 'Segoe UI', sans-serif;
      font-size: 24px;
      color: #333;
      text-align: center;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
      z-index: 1000;
      min-width: 350px;
      max-width: 90vw;
    }
  </style>
</head>
<body>
  <!-- Overlay Box -->
  <div class="overlay-box" id="info-box">
    <strong>Date: </strong>`+global.DATEMap+`<br>
    <strong>Lat: </strong>`+global.LATMap+`, <strong>Lon: </strong> `+global.LONMap+`<br>
    <strong>Speed: </strong>`+global.SPEEDMap+` km/h<br>
    <strong>Altitude: </strong>`+global.ALTMap+` m
  </div>

  <!-- Map Container -->
  <div id="map"></div>

  <script
    src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"
    crossorigin=""
  ></script>
  <script>
    const coords = [`+global.LATMap+`, `+global.LONMap+`];

    const map = L.map('map').setView(coords, 16);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; OpenStreetMap contributors'
    }).addTo(map);

    const marker = L.marker(coords).addTo(map);

  </script>
</body>
</html>`

  res.send(HTML)
})

app.listen(3000, () => {
  console.log("http server is running on port 3000")
})
