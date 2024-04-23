var dataSet;

function createDataSet() {
  dataSet = [
    ['WLAN_ESSID', 'xxxx', 'SSID'],
    ['WLAN_Password', '100', 'Password'],
    ['IP_Inverter', '', 'IP-Adresse des Inverters'],
    ['Heizstableistung', '1000', 'Heizstableistung in Watt'],
    ['Ausschalt_Temperatur', '80', 'Maximal erlaubte Temperatur'],
    ['Einschalt_Temperatur', '40', 'Temperatur, bei der eingeschaltet werden muss'],
    ['Mindest_Einspeisung', '100', 'Strom, der vom Heizregler nicht verwendet werden darf (in W)'],
    ['Speicher', 'n', 'Externer Speicher steht zur Verfügung (j,n)'],
    ['Speicher_Prioritaet', '1', '1: Externer Speicher vorrangig, 2: nachrangig'],
    /*  ['Mindeslaufzeit_Digital', '2', 'Mindeslaufzeit vor einer Änderung der digitalen Ports (in ms)'],
     ['Mindeslaufzeit_Phase', '2', 'Mindeslaufzeit, in der eine Phase eingeschaltet ist (in ms)'], */
    ['Mindeslaufzeit_Regler', '2', 'Zeitperiode, in der Änderungen vom Regler nicht berücksichtigt werden (in s)'],
    /* ['Ausgangsregler (P-Anteil)', '0.9', 'PI-Regler für den 0-100 % Ausgang (0.0 -1.0)'],
    ['Ausgangsregler (I-Anteil)', '0.1', 'PI-Regler für den 0-100 % Ausgang (0.0 -1.0)'],
    ['Ausgangsregler (D-Anteil)', '0.0', 'PI-Regler für den 0-100 % Ausgang (0.0 - 1.0)'], */
    ['Amis Reader Host (TCP/IP)', 'AmisReader-Host', 'Amis Reader Host (TCP/IP) | ---'],
    ['Amis Reader Key', 'Key', 'Amis Reader kEY | ---'],
    ['MQTT-Server', 'MQTT-Host', 'MQTT-Server | ---'],
    ['MQTT-User', 'User', 'MQTT-User'],
    ['MQTT-Password', 'Password', 'MQTT-Password'],
    ['Influx-Server', 'Influx-Host', 'Influx-Server | ---'],
    ['Influx-Token', 'Token', 'Influx-Token'],
    ['Influx-Org', 'Org', 'Influx-Org'],
    ['Influx-Bucket', 'Bucket', 'Influx-Bucket'],
    ['SIM_Additional_Load', '0.0', 'Simulation: zusätzliche Last'],
    ['Force Heizpatrone', '10', '0: Aus, |1|2|3: Ein, 10: Auto'],
  ];


}



function showError(text) {

  $('#error').animate({
    backgroundColor: '#ddd',
  }, 1000, function () {
    $(this).css({
      "background-color": 'red',
      "color": "white",
      "margin-left": "10px"
    });
  });
  $('#error').val(text)
  $('#error').show()
  return false

}

function showAjaxError(text) {

  $('#errorAjax').animate({
    backgroundColor: '#ddd',
  }, 3000, function () {
    $(this).css({
      "background-color": 'red',
      "color": "white",
      "margin-left": "10px"
    });
  });
  $('#errorAjax').val(text)
  $('#errorAjax').show()
  return false
}

function showAjaxSuccess(text) {

  $('#errorAjax').animate({
    backgroundColor: '#ddd',
  }, 1000, function () {
    $(this).css({
      "background-color": 'green',
      "color": "white",
      "margin-left": "10px"


    });
  });
  $('#errorAjax').val(text)
  $('#errorAjax').show()
  return false
}


function evalIt(value, index) {
  console.log("val: " + value + " , ind: " + index)
  let ipaddress =
    /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/;

  if (value == "")
    return showError("Feld kann nicht leer sein.")
  let nu = 0, fnum = 0.0
  $('#error').hide()
  switch (index) {
    case 2: if (!value)   // ip inverter
      return showError("Eingabe (TCP-Adresse) erforderlich")
      if (!ipaddress.test(value))
        return showError("Keine gültige IP-Adresse!")

      break;
    case 3: if (isNaN(value))  // heizstabLeistunmg
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu >= 10000)
        return showError("Wertebereich ungültig (>0 und <= 10000)")
      break;
    case 4: if (isNaN(value)) // ausschalttemp
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu >= 90)
        return showError("Wertebereich ungültig (>0 und <= 90)")
      break;
    case 5: if (isNaN(value)) // einspeisT
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu >= 90)
        return showError("Wertebereich ungültig (>0 und <= 90)")
      break;
    case 6: if (isNaN(value)) // einspesung muss
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu >= 20000)
        return showError("Wertebereich ungültig (>0 und <= 20000)")
      break;
    case 7: if (!value) // externer speicher
      return showError("Feld kann nicht leer sein.")
      if (value == 'j') return true;
      if (value == 'n') return true;
      return showError("Antwort kann nur 'j' oder 'n' sein.")
      break;
    case 8: if (isNaN(value)) // priorität einspeisung
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu > 3)
        return showError("Wertebereich ungültig (>0 und <= 3)")
      break;
    case 9: if (isNaN(value)) // Mindestlaufzeit regler
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1)
        return showError("Wertebereich ungültig (>0 )")
      break;
    case 10:
      if (value == "---") // no mqtt server
        break;
      if (!ipaddress.test(value))
        return showError("Keine gültige IP-Adresse!")
      break;
    case 11:  // amis key
      if (value.length != 32)
        return showError("Key muss eine Länge von 32 Zeichen haben") // amis key
      break;
    case 12:
      if (value == "---") // no mqtt server
        break;
      if (!ipaddress.test(value))
        return showError("Keine gültige IP-Adresse!")
      break;
    case 13:  // mqtt user
      break;
    case 14:  // mqtt passwd
      break;
    case 15:
      if (value == "---") // no influx server
        break;
      break;
    case 16:  // influx token
      break;
    case 17:  // influx bucket
      break;

    case 17: if (isNaN(value)) //wimulation additional load
      return showError("Numerische Eingabe erforderlich")
      fnum = parseFloat(value)
      if (fnum < 0.0)
        return showError("Wertebereich ungültig (>=0.0)")
      break;

    case 18: if (isNaN(value)) // simulation bias power
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nm < 0)
        return showError("Wertebereich ungültig (>=0)")
      switch (nu) {
        case 0: break;
        case 1: break;
        case 2: break;
        case 3: break;
        case 10: break;
        default: return showError("Wertebereich ungültig - siehe Documenatation)")
      }
      break;


  }
  return true
}
var currentIndex = 0
var dataTable = null

function initHandler() {
  $('#addRow').on('click', function (e) {
    e.preventDefault()
    //when submit button acts to submit edits
    if ($(this).attr('action') == 'confirmEdit') {
      val = $('#theValue').val();
      if (!evalIt(val, currentIndex))
        return

      $('#editBlock').hide();

      //dataTable.row($(this).attr('rowindex')).data([$("#theTitle").val(), $("#theValue").val()]).draw();
      dataSet[currentIndex][1] = val;
      dataTable.row(currentIndex).invalidate().draw()
    }
    //clean up form, switch it to default state
    $('#theTitle').val("");
    $('#theValue').val("");

  });


  //'Edit' button click handler
  $('#stamm').on('click', 'tbody tr button[action="edit"]', function (e) {
    e.preventDefault();
    $('#editBlock').show();
    $('#theValue').focus();
    $('#error').hide();
    $('#errorAjax').hide();
    const row = dataTable.row($(event.target).closest('tr'));
    currentIndex = row.index()
    //get affected row().index() and append that to 'Submit' button attributes
    //you may use global variable for that purpose if you prefer
    $('#addRow').attr('rowindex', row.index());
    //switch 'Submit' button role to 'confirmEdit'
    $('#addRow').attr('action', 'confirmEdit');
    //set up 'Type' and 'Amount' values according to the selected entry
    $('#theTitle').val(row.data()[0]);
    $('#theValue').val(row.data()[1]);
  });
}

function storeDataAjax() {
  $('#form_store').on('submit', function (e) {
    //$(('button[id^="store"]')).click(function () {
    e.preventDefault();
    //let dataTable = $('#stamm');
    let numberRows = dataTable.rows().count()
    let dataAjax = {}
    for (jj = 0; jj < numberRows; jj++) {
      let row = dataTable.row(jj);
      // key::value
      dataAjax[row.data()[0]] = row.data()[1]
    }


    console.log("Ajax - call EP .... /storeSetup")
    $('#errorAjax').hide();

    $.ajax({
      type: 'POST',
      url: '/storeSetup',
      data: JSON.stringify(dataAjax),
      contentType: 'application/json',
    })
      .done((dataC) => {
        //const answ = JSON.parse(dataC);
        if (dataC.done == 0)
          showAjaxError(dataC.error);
        else {
          showAjaxSuccess("Setup Daten wurden gespeichert / µController wird neu gestartet (10 sec)/ Browserfenster schließen / Display beachten für Web-Zugang")
        }
        //alert("Daten wurden erfolgreich gespeichert!\nBei Änderung der IP-Adresse wird neu gebootet!")
        //console.log({ dataC });
      })
      .fail((err) => {
        console.error(err);
        showAjaxError("Kommunikationsfehler ist aufgetreten!");
      })
      .always(() => {
        console.log('always called');
      });

  });
}

function replace(index, val) {
  //let cell = $('#stamm tr:eq(' + index + ') td:eq(1)');
  const row = dataTable.row(index)
  // update model
  dataSet[index][1] = val
  row.invalidate().draw()
}

function renewTable() {

  $.ajax({
    type: "GET",
    url: "/getSetup", //http://10.0.0.59
    async: true,
    contentType: "application/json; charset=utf-8",
    dataType: "json",
    success: function (data) {
      if (data !== null) {

        console.log("DONE")
        console.log(data["error"])
        replace(0, data["WLAN_ESSID"])
        replace(1, data["WLAN_Password"])
        replace(2, data["IP_Inverter"])
        replace(3, data["Heizstableistung"])
        replace(4, data["Ausschalt_Temperatur"])
        replace(5, data["Einschalt_Temperatur"])
        replace(6, data["Mindest_Einspeisung"])
        replace(7, data["Speicher"])
        replace(8, data["Speicher_Prioritaet"])
        replace(9, data["Mindeslaufzeit_Regler"])
        replace(10, data["Amis Reader Host (TCP/IP)"])
        replace(11, data["Amis Reader Key"])

        replace(12, data["MQTT-Server"])
        replace(13, data["MQTT-User"])
        replace(14, data["MQTT-Password"])

        replace(15, data["Influx-Server"])
        replace(16, data["Influx-Token"])
        replace(17, data["Influx-Org"])
        replace(18, data["Influx-Bucket"])

        replace(19, data["SIM_Additional_Load"])
        replace(20, data["Force Heizpatrone"])

      } else {
        console.log("Some error ")
      }
    },
    error: function Error(result, error) {
      alert("error " + result.status + " " + result.statusText);
    }
  });
}



function buildStaticTable() {
  createDataSet()
  dataTable = $('#stamm').DataTable
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
          width: "30%",

        },
        {
          title: 'Wert',
          render: dataTable => `${dataTable}<td><button action="edit">Edit</button></td>`
        },
        {
          title: 'Bemerkung',
          width: "40%",

        }


      ],
      columnDefs: [
        {
          target: 1,
          visible: true,
          searchable: false
        },

      ],
      data: dataSet
    });


}


