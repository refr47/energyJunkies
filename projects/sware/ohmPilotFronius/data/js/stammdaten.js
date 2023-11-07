document.title = 'Setup';
document.referrer = 'setup'
var dataTable = null;
function showError(text) {

  $('#error').animate({
    backgroundColor: '#ddd',
  }, 1000, function () {
    $(this).css({
      "background-color": 'red',
      "color": "white"
    });
  });
  $('#error').val(text)
  $('#error').show()
  return false

}
function showAjaxError(text) {

  $('#errorAjax').animate({
    backgroundColor: '#ddd',
  }, 1000, function () {
    $(this).css({
      "background-color": 'red',
      "color": "white"
    });
  });
  $('#errorAjax').val(text)
  $('#errorAjax').show()
  return false

}
function evalIt(value, index) {
  console.log("val: " + value + " , ind: " + index)
  if (value == "")
    return showError("Feld kann nicht leer sein.")
  let nu = 0, fnum = 0.0
  $('#error').hide()
  switch (index) {
    case 2: if (!value)
      return showError("Eingabe (TCP-Adresse) erforderlich")

      let ipaddress =
        /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/;

      if (!ipaddress.test(value))
        return showError("Keine gültige IP-Adresse!")

      break;
    case 3: if (isNaN(value))  // hyterese
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
    case 5: if (isNaN(value)) // einspesung muss
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu >= 20000)
        return showError("Wertebereich ungültig (>0 und <= 20000)")
      break;
    case 6: if (!value) // externer speicher
      return showError("Feld kann nicht leer sein.")
      if (value == 'j') return;
      if (value == 'n') return;
      return showError("Antwort kann nur 'j' oder 'n' sein.")
      break;
    case 7: if (isNaN(value)) // priorität einspeisung
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1 || nu > 3)
        return showError("We  $('#errorAjax').hide();rtebereich ungültig (>0 und <= 3)")
      break;
    case 8: if (isNaN(value)) // Mindestlaufzeit digitaler kanal
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1)
        return showError("Wertebereich ungültig (>0 )")
      break;
    case 9: if (isNaN(value)) // Mindestlaufzeit phase
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1)
        return showError("Wertebereich ungültig (>0 )")
      break;
    case 10: if (isNaN(value)) // Mindestlaufzeit regler
      return showError("Numerische Eingabe erforderlich")
      nu = parseInt(value)
      if (nu < 1)
        return showError("Wertebereich ungültig (>0 )")
      break;
    case 11: if (isNaN(value)) // pid regler, p anteil
      return showError("Numerische Eingabe erforderlich")
      fnum = parseFloat(value)
      if (fnum < 0.0 || fnum > 1.0)
        return showError("Wertebereich ungültig (>=0.0 und <= 1.0)")
      break;
    case 12: if (isNaN(value)) // pid i anteil
      return showError("Numerische Eingabe erforderlich")
      fnum = parseFloat(value)
      if (fnum < 0.0 || fnum > 1.0)
        return showError("Wertebereich ungültig (>=0.0 und <= 1.0)")
      break;
      break;
    case 13: if (isNaN(value)) // pid d anteil
      return showError("Numerische Eingabe erforderlich")
      fnum = parseFloat(value)
      if (fnum < 0.0 || fnum > 1.0)
        return showError("Wertebereich ungültig (>=0.0 und <= 1.0)")
      break;
      break;

  }
  return true
}
$(document).ready(function () {
  $("#headerContent").load("headerF.html #headerSection");
  document.title = 'Setup';
  $('#form_store').on('submit', function (e) {
    //$(('button[id^="store"]')).click(function () {
    e.preventDefault();
    //let dataTable = $('#stamm');
    let numberRows = dataTable.rows().count()
    let dataAjax = {}
    for (jj = 0; jj < numberRows; jj++) {
      let row = dataTable.row(jj);
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
        else
          alert("Daten wurden erfolgreich gespeichert!\nBei Änderung der IP-Adresse wird neu gebootet!")
        console.log({ dataC });
      })
      .fail((err) => {
        console.error(err);
        showAjaxError("Kommunikationsfehler ist aufgetreten!");
      })
      .always(() => {
        console.log('always called');
      });

  });


  var currentIndex = 0
  dataTable = $('#stamm').DataTable
    ({
      "dom": 'r',
      "lengthChange": false,
      "info": false,
      "bPaginate": false,
      "ordering": false,
      "iDisplayLength": 100,
      "paging": false,
      "scrollCollapse": true,
      "scrollY": '200px',
      columns: [
        { title: 'Titel' },
        {
          title: 'Wert',
          render: dataTable => `${dataTable}<td><button action="edit">Edit</button></td>`
        },


      ],
      columnDefs: [
        {
          target: 1,
          visible: true,
          searchable: false
        },

      ]
    });
  $(".dataTables_filter").hide();
  $('#editBlock').hide();

  $('#addRow').on('click', function (e) {
    e.preventDefault()
    //when submit button acts to submit edits
    if ($(this).attr('action') == 'confirmEdit') {
      //change affected row data and re-draw the table

      //console.log("update ..." + currentIndex)
      val = $('#theValue').val();
      if (!evalIt(val, currentIndex))
        return

      $('#editBlock').hide();
      dataTable.row($(this).attr('rowindex')).data([$("#theTitle").val(), $("#theValue").val()]).draw();
    }
    //clean up form, switch it to default state
    $('#theTitle').val("");
    $('#theValue').val("");

  });


  //'Edit' button click handler
  $('#stamm').on('click', 'tbody tr button[action="edit"]', function (e) {
    //get affected row entry
    e.preventDefault();
    $('#editBlock').show();
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
});