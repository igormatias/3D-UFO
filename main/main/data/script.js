let brightness = document.getElementById("brightness");

brightness.addEventListener("click", function(e) {
    sendAJAX("brightness?value=" + this.value);
});

function nextMode(){
    sendAJAX("nextMode");
}

function ledOFF(){
    sendAJAX("ledOff");
}

function sendAJAX(val){
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.status == 200 && this.readyState == 4) {
            console.log("AJAX has sent!");
        }
    }
    xhttp.open("GET", val, true);
    xhttp.send();
}

function colorSet() {
    let r = document.getElementsByName("r")[0].value;
    let g = document.getElementsByName("g")[0].value;
    let b = document.getElementsByName("b")[0].value;
    sendAJAX("colourMode?r=" + r +"&g=" + g + "&b=" + b);
}