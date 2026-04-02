// output.js
var dataSetOut;
var gateway = `ws://${window.location.hostname}/ws`;
//var gateway = `ws://${"10.0.0.4"}/ws`;
var websocket;
var dataTableMobil;


const HEIZPATRONE_L1 = 1;
const HEIZPATRONE_L2 = 2;
const AKKU_AVAILABLE = 3;
const STATE_CARDWRITE = 4;
const STATE_MODBUS = 5;
const STATE_FLASH = 6;
const STATE_TEMPSENSOR = 7;
const STATE_HEATING = 8;


function addErrors(errorList) {
  $("#errorL").remove();
  // $("#a").css('background-color', 'red');
  const $ul = $('<ul>', { id: "errorL" }).appendTo('.flex-container');
  $.each(errorList, function (index, errorName) {
    console.log("Appüend: " + errorName)
    // $('#errorL ul').append('<li><class="errListElem">'+errorName+'</li>')
    $('<li>').text(errorName).appendTo($ul);
  });
  $(".flex-container").css('background-color', 'red');
}


function interpretError(errorBitVektor) {
  let errVek = [];
  if ((errorBitVektor & (1 << STATE_CARDWRITE)) != 0)
    errVek.push("SD-Kartenleser funktioniert nicht.")
  if ((errorBitVektor & (1 << STATE_MODBUS)) != 0)
    errVek.push("Modbus funktioniert nicht - Wechselrichter kann nicht erreicht werden.")
  if ((errorBitVektor & (1 << STATE_FLASH)) != 0)
    errVek.push("Flashspeicher funktioniert nicht.")
  if ((errorBitVektor & (1 << STATE_TEMPSENSOR)) != 0)
    errVek.push("Probleme mit der Temperatursensorik oder MaxWert erreicht.")
  if ((errorBitVektor & (1 << STATE_HEATING)) != 0)
    errVek.push("Probleme mit der Temperatursensorik oder MaxWert erreicht.")
  if (errVek.length > 0)
    addErrors(errVek);
}

function Trenner(number) {
  // Info: Die '' sind zwei Hochkommas
  number = '' + number;
  if (number.length > 3) {
    var mod = number.length % 3;
    var output = (mod > 0 ? (number.substring(0, mod)) : '');
    for (i = 0; i < Math.floor(number.length / 3); i++) {
      if ((mod == 0) && (i == 0))
        output += number.substring(mod + 3 * i, mod + 3 * i + 3);
      else
        // hier wird das Trennzeichen festgelegt mit '.'
        output += '.' + number.substring(mod + 3 * i, mod + 3 * i + 3);
    }
    return (output);
  }
  else return number;
}

function replace(index, val, fixIt) {

  //let cell = $('#stamm tr:eq(' + index + ') td:eq(1)');
  const row = dataTableMobil.row(index)
  // update model
  if (fixIt == true) {
    dataSetOut[index][1] = Trenner(Math.trunc(val < 0 ? val - 0.5 : val + 0.5));



  } else
    dataSetOut[index][1] = val
  //cell.css("background-color", "red")

  row.invalidate().draw()
}


/*   *******************************
                WEBsocks  
  *******************************
  */


function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage; // <-- add this line
}

function onOpen(event) {
  console.log('Connection opened');
  $("#isConn").html("\u2714")
}
 
function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
  $("#isConn").html("\u2716")
}
function onMessage(event) {
  console.log("Got event ")
  $("#isUpdate").html("\u21C5")

  let dataJson = JSON.parse(event.data);
  let live = dataJson.live;
  let log = dataJson.log;
  

  //console.log(data)
  setTimeout(replaceDataReceivedSym, 1000);

  function updateTable(entries) {
    const tbody = document.querySelector("#logTable tbody");

    // 🧹 optional: alte Daten löschen
    tbody.innerHTML = "";

    entries.forEach(e => {
      const row = document.createElement("tr");

      row.innerHTML = `
            <td>${new Date(e.ts * 1000).toLocaleTimeString()}</td>
            <td>${e.l1}</td>
            <td>${e.l2}</td>
            <td>${e.pwm}</td>
            <td>${e.temp.toFixed(1)}</td>
        `;

      tbody.appendChild(row);
    });
  }


  let errorBitVektor = live["FE"]
  if (errorBitVektor == 0)
    $("#errorL").remove();
  else
    interpretError(errorBitVektor);
  if ((errorBitVektor & (1 << HEIZPATRONE_L1)) != 0)
    dataSetOut[5][1] = "1"
  else
    dataSetOut[5][1] = "0"
  let row = dataTableMobil.row(5)
  row.invalidate().draw();
  if ((errorBitVektor & (1 << HEIZPATRONE_L2)) != 0)
    dataSetOut[6][1] = "1"
  else
    dataSetOut[6][1] = "0"
  row = dataTableMobil.row(6)
  row.invalidate().draw();
  if ((errorBitVektor & (1 << AKKU_AVAILABLE)) != 0)
    dataSetOut[9][1] = "1"
  else
    dataSetOut[9][1] = "0"
  row = dataTableMobil.row(9)
  row.invalidate().draw();
  replace(0, live["PR"], true); // ProdukTION
  replace(1, live["EV"], true); // Verbrauch
  if (live["EINS"] > 0.0)
    dataSetOut[2][0] = "Bezug"
  else 
    dataSetOut[2][0] = "Einspeisung"
  replace(2, live["EINS"], true); // Einspeisung
  if (live["AKKU"] > 0.0)
    dataSetOut[3][0] = "Akku entladen"
  else
    dataSetOut[3][0] = "Akku laden"
  replace(3, live["AKKU"], false)
  replace(4, live["TPS"], false); // Sensorik Temp
  replace(7, live["HL3"], false); // pwm 
  replace(10, live["AkStat"], true); // Akku Status
  replace(11, live["EpsilonPin"], false); // Sim Bias
  replace(12, live["Force Heizpatrone"], false); // Sim Load

  let count = dataJson.log.count;
  if (count > 0) {
    console.log("Log-Entries: " + count);
    updateTable(dataJson.log.entries);
  } else {
    console.log("No log entries received.");
    let entries = dataJson.log.entries;
  }
 
}

function replaceDataReceivedSym() {
  $("#isUpdate").html("\u2718")
}




function createDataSetM() {
  dataSetOut = [
    ['Produktion', '0', "Watt"],
    ['Verbrauch', '0', "Watt"],
    ['Einspeisung/Bezug', '0', "Watt"],
    ['Akku', '0', "Watt"],
    ['Temperatur', '49', "Grad"],
    ['Pufferspeicher L1', '1', "Aus:0, Ein: 1"],
    ['Pufferspeicher L2', '1', "Aus:0, Ein: 1"],
    ['Pufferspeicher L3', '10', "PWM"], //7
    ['Pufferspeicher reservier', '0', "W"], //8
    ['Speicher', 'j', "Aus:0, Ein: 1"], // 9
    ['Speicher Zustand (%)', '0.0', "%"], // 10
    ['EpsilonPin', '0.05', " "], //11
    ['Force Heizpatrone', '0.0 kW', "kW"], //12
  ];


}

function buildStaticTableM() {
  createDataSetM()
  $("#wsHost").html(gateway)
  /*  let xx=['Modbus: keine Verbindung','Temperatur: Soll erreicht']
   addErrors(xx); */
  dataTableMobil = $('#details').DataTable
    ({

      "rowCallback": function (row, data, displayNum, displayIndex, dataIndex) {
        //console.log(data[0],displayNum,displayIndex,dataIndex)

        // console.log(data[1])
        if (data[0] == "Produktion" && data[1] > 0.0) {
          $('td:eq(1)', row).css('background-color', 'green')
          /* data[1] = data[1] + "W"
          console.log($(td).html()) */
        }
        if (data[0] == "Bezug" && data[1] > 0.0)
          $('td:eq(1)', row).css('background-color', 'red')
        if (data[0] == "Einspeisung" && data[1] < 0.0)
          $('td:eq(1)', row).css('background-color', 'green')
      },
      "bProcessing": true,
      "lengthChange": false,
      "info": false,
      "bPaginate": true,
      "ordering": false,
      "responsive": true,
      columns: [
        {
          title: 'Titel',
          width: "40%",
        },
        {
          title: 'Ausprägung'
        },
        {
          title: 'Einheit'
        }
      ],
      columnDefs: [
        {
          target: 1,
          visible: true,
          searchable: false
        },
      ],
      data: dataSetOut
    });


}

