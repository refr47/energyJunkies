var dataSetOut;
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var dataTableMobil;


const HEIZPATRONE_L1 = 1;
const HEIZPATRONE_L2 = 2
const AKKU_AVAILABLE = 3
const STATE_CARDWRITE = 4
const STATE_MODBUS = 5
const STATE_FLASH = 6
const STATE_TEMPSENSOR = 7


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
    errVek.push("Probleme mit der Temperatursensorik.")
  if (errVek.length > 0)
    addErrors(errVek);
}

function replace(index, val) {
  //let cell = $('#stamm tr:eq(' + index + ') td:eq(1)');
  const row = dataTableMobil.row(index)
  // update model
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

  let data = JSON.parse(event.data);
  console.log(data)
  setTimeout(replaceDataReceivedSym, 1000);

  let errorBitVektor = data["FE"]
  if (errorBitVektor == 0)
    $("#errorL").remove();
  else
    interpretError(errorBitVektor);
  if ((errorBitVektor & (1 << HEIZPATRONE_L1)) != 0)
    dataSetOut[4][1] = "1"
  else
    dataSetOut[4][1] = "0"
  let row = dataTableMobil.row(4)
  row.invalidate().draw();
  if ((errorBitVektor & (1 << HEIZPATRONE_L2)) != 0)
    dataSetOut[5][1] = "1"
  else
    dataSetOut[5][1] = "0"
  row = dataTableMobil.row(5)
  row.invalidate().draw();
  replace(0, data["PR"]); // ProdukTION
  replace(1, data["EV"]); // Verbrauch
  if (data["EINS"] > 0.0)
    dataSetOut[1][0] = "Bezug"
  else
    dataSetOut[1][0] = "Einspeisung"
  replace(2, data["EINS"]); // Einspeisung
  replace(3, data["TPS"]); // Sensorik Temp
  replace(6, data["HL3"]); // pwm 
}

function replaceDataReceivedSym() {
  $("#isUpdate").html("\u2718")
}




function createDataSetM() {
  dataSetOut = [
    ['Produktion', '3589 W'],
    ['Verbrauch', '1000'],
    ['Einspeisung', '2589 '],
    ['Temperatur', '49'],
    ['Pufferspeicher L1', '1'],
    ['Pufferspeicher L2', '1'],
    ['Pufferspeicher L3', '10'],
    ['Pufferspeicher reservier', '0'],
    ['Speicher', 'j'],
    ['Speicher', 'n', 'Externer Speicher steht zur Verfügung (j,n)'],
    ['Speicher Kapazität', '13 kW'],
    ['Speicher Zustand', '3 kW'],
    ['Speicher Laden', '200 W'],
  ];


}

function buildStaticTableM() {
  createDataSetM()
  $("#wsHost").html(gateway)
  /*  let xx=['Modbus: keine Verbindung','Temperatur: Soll erreicht']
   addErrors(xx); */
  dataTableMobil = $('#details').DataTable
    ({
      "dom": 'Bfrtip',
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

