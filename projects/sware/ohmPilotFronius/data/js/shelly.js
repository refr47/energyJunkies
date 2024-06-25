var dataSetShellyDevices;
var dataTableShellyDevices;

function showShellyDevices() {

    $.ajax({
        type: "GET",
        url: "http://10.0.0.59/getShellyDevicesTree", //http://10.0.0.59
        async: true,
        contentType: "application/json; charset=utf-8",
        dataType: "json",
        success: function (data) {
            if (data !== null) {

                console.log("DONE")


            } else {
                console.log("Some error ")
            }
        },
        error: function Error(result, error) {
            alert("error " + result.status + " " + result.statusText);
        }
    });
}
function createDataSetDevices() {
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
        ['SIM_Additional_Load', '0.0 kW', "kW"], //11
        ['Force Heizpatrone', '0.0 kW', "kW"], //12
    ];

}

function buildStaticTableShellyDevs() {
    createDataSetDevices()
    showShellyDevices()

    dataTableShellyDevices = $('#allShellyDevices').DataTable
        ({
            "bProcessing": true,
            "lengthChange": false,
            "info": false,
            "bPaginate": true,
            "ordering": false,
            "responsive": true,
            columns: [
                {
                    title: 'Name',
                    width: "40%",
                },
                {
                    title: 'Mac-Adresse'
                },
                {
                    title: 'IP-Adresse'
                }
            ],
            columnDefs: [
                {
                    target: 1,
                    visible: true,
                    searchable: false
                },
            ],
            data: dataSetShellyDevices
        });


}

var $tooltip = $('#tooltip');

$("#shellyInitButton").on("click", function () {
    alert("Handler for `click` called.");
});

$('#shellyInitButton').hover(
    function (e) { // Mouse enter
        $tooltip.css({
            display: 'block',
            top: e.pageY + 10,
            left: e.pageX + 10
        });
    },
    function () { // Mouse leave
        $tooltip.hide();
    }
).mousemove(function (e) {
    $tooltip.css({
        top: e.pageY + 10,
        left: e.pageX + 10
    });
});



