

var dataTableI;
var dataSetI;



function createDataSetI() {
  dataSetI = [
    ['Fronius', '0', ""], // 0
    ['AmisReader', '0', ""], //1
    ['CardReader', '0', ""], //2
    ['Akku', '0', ""], // 3
    ['Flash FS', '0', ""],
    ['Influx', '0', ""],
    ['Modbus', '0', ""],
    ['MQTT', '0', ""], //7
    ['TempSensor', '0', ""], //8
  ];
}

function buildStaticTableI() {
  createDataSetI()
  dataTableI = $('#overview').DataTable
    ({
      "bProcessing": true,
      "lengthChange": false,
      "info": false,
      "bPaginate": true,
      "ordering": false,
      "responsive": true,
      columns: [
        {
          title: 'Komponente',
          width: "40%",
        },
        {
          title: 'In Betrieb'
        },
        {
          title: 'Ressource'
        }
      ],
      columnDefs: [
        {
          target: 1,
          visible: true,
          searchable: false
        },
      ],
      data: dataSetI
    });


}


function addErrorsI(errorList) {
  $("#errorL").remove();
  // $("#a").css('background-color', 'red');
  const $ul = $('<ul>', { id: "errorL" }).appendTo('.flex-container');
  $.each(errorList, function (index, errorName) {
    console.log("Append: " + errorName)
    // $('#errorL ul').append('<li><class="errListElem">'+errorName+'</li>')
    $('<li>').text(errorName).appendTo($ul);
  });
  $(".flex-container").css('background-color', 'red');
}
function replaceInd(index, val, val1) {

  //let cell = $('#stamm tr:eq(' + index + ') td:eq(1)');
  const row = dataTableI.row(index)
  // update model
  dataSetI[index][1] = val == true ? "j" : "n"
  dataSetI[index][2] = val1

  //cell.css("background-color", "red")
  row.invalidate().draw()
}

function getData() {
  $.getJSON("/getOverview").done(function (data) {
    replaceInd(0, data["FR"], data["FRIP"]);
    replaceInd(1, data["AM"], data["AMIP"]);
    replaceInd(2, data["CR"], " ");
    replaceInd(3, data["AK"], data["AKK"]);
    replaceInd(4, data["FL"], " ");
    replaceInd(5, data["IN"], data["INIP"]);
    replaceInd(6, data["MB"], data["MBIP"]);
    replaceInd(7, data["MQ"], data["MQIP"]);
    replaceInd(8, data["TEMP"], data["TEMPV"]);
    console.log("parsed ", data)
  })
    .fail(function (jqxhr) {
      let errVek = [];
      errVek.push("Fehler beim Abrufen der Daten.");
      addErrorsI(errVek);
      console.log(jqxhr.responseText);
    });
}

