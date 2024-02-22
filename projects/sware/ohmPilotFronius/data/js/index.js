

var dataTableIndex;
var dataSetIndex;



function createDataSetI() {
    dataSetIndex = [
      ['Fronius', '0', ""],
      ['AmisReader', '0', ""],
      ['CardReader', '0', ""],
      ['Akku', '0', ""],
      ['Flash FS', '0', ""],
      ['Influx', '0', ""],
      ['Modbus', '0', ""],
      ['MQTT', '0', ""], //7
      ['TempSensor', '0', ""], //8
    ];
  }
  
  function buildStaticTableI() {
    createDataSetI()
     dataTableIndex = $('#overview').DataTable 
      ( {"bProcessing": true,
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
        data: dataSetIndex
      });
  
  
  }
  

  function getData() {
    $.getJSON("/getOverview").done(function(data){
        let parsed = JSON.parse(data);
       console.log("parsed ",parsed)
     })
     .fail(function(jqxhr){
        console.log(jqxhr.responseText);
     });
  }

  