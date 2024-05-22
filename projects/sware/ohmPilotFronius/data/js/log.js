function fetchRunningLogs() {

    $.ajax({
        type: "GET",
        url: "/serial", //http://10.0.0.59
        async: true,
        contentType: "text/html; charset=utf-8",
        dataType: "text"
        success: function (data) {
            if (data !== null) {

                console.log("Log fetch DONE")
                $('#output').append(data);


            } else {
                console.log("Some error ")
            }
        },
        error: function Error(result, error) {
            alert("error " + result.status + " " + result.statusText);
        }
    });
}

setInterval(fetchRunningLogs, 1000); // Aktualisiere jede Sekunde