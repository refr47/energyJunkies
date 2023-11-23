var dataSetOut;

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
    ['Alarm',""],
    ['Fehler', ' CardReader funktioniert nicht, Zeitserver nicht erreichbar'],
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