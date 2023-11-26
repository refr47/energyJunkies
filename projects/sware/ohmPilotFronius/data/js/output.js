var dataSetOut;
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;



function replace(index, val) {
  //let cell = $('#stamm tr:eq(' + index + ') td:eq(1)');
  const row = dataTable.row(index)
  // update model
  dataSetOut[index][1] = val
  row.invalidate().draw()
}


/*   *******************************
                WEBsocks  
  *******************************
  */


  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
    $("#isConn").html("\u10004")
  }

  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
    $("#isConn").html("\u2714")
  }
  function onMessage(event) {
    console.log("Got event ")
    $("#isUpdate").html("\u21C5")
    
    let data = JSON.parse(event.data);
    console.log(data)
    setTimeout(replaceDataReceivedSym, 1000);
    replace(0, data["PR"]); // ProdukTION
    replace(0, data["EV"]); // ProdukTION

/*     var state;
    if (event.data == "1"){
      state = "ON";
    }
    else{
      state = "OFF";
    }
    document.getElementById('state').i
    nnerHTML = state; */
  }

  function replaceDataReceivedSym() {
    $("#isUpdate").html("\u2718")
  }

  function addErrors(errorList) {
    $("#errorL").remove();
   // $("#a").css('background-color', 'red');
    const $ul = $('<ul>', { id: "errorL" }).appendTo('.flex-container');
    $.each(errorList, function(index,errorName) {
      console.log("Appüend: " + errorName)
     // $('#errorL ul').append('<li><class="errListElem">'+errorName+'</li>')
     $('<li>').text(errorName).appendTo($ul);
    });
    $(".flex-container").css('background-color', 'red');
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


var dataTableMobil;

function buildStaticTableM() {
    createDataSetM()
    $("#wsHost").html(gateway)
    let xx=['Modbus: keine Verbindung','Temperatur: Soll erreicht']
    addErrors(xx);
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

