let brightness = document.getElementById("brightness");
let brightnessLabel = document.getElementById("brightnessLabel");
let rSlider = document.getElementById("r");
let gSlider = document.getElementById("g");
let bSlider = document.getElementById("b");
let rlabel = document.getElementById("rlabel");
let glabel = document.getElementById("glabel");
let blabel = document.getElementById("blabel");
let rgbBar = document.getElementById("rgbBar");

brightness.addEventListener("click", changeBrightness);
brightness.addEventListener("touchend", changeBrightness);

rSlider.addEventListener("click", changeColor);
gSlider.addEventListener("click", changeColor);
bSlider.addEventListener("click", changeColor);
rSlider.addEventListener("touchend", changeColor);
gSlider.addEventListener("touchend", changeColor);
bSlider.addEventListener("touchend", changeColor);

function updateHTML() {
    let r = rSlider.value;
    let g = gSlider.value;
    let b = bSlider.value;
    let bri = brightness.value;
    rlabel.innerHTML = r;
    glabel.innerHTML = g;
    blabel.innerHTML = b;
    brightnessLabel.innerHTML = bri;
    let newColor = "rgb(" + r + ", " + g + ", " + b + ")";
    rgbBar.style.backgroundColor = newColor;
    rgbBar.innerHTML = newColor;
    if ((Number(r) + Number(g) + Number(b) / 3) < 128) {
        rgbBar.style.color = "white";
    }
    else {
        rgbBar.style.color = "black";
    }
}

function changeColor() {
    updateHTML();
    colourSet();
}

window.onload = function () {
    updateHTML();

}
function changeBrightness(e) {
    updateHTML();
    sendAJAX("brightness?value=" + brightness.value);
}

function nextMode() {
    sendAJAX("nextMode");
}

function ledOFF() {
    sendAJAX("ledOff");
}

function sendAJAX(val) {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.status == 200 && this.readyState == 4) {
            console.log("AJAX has sent!");
        }
    }
    xhttp.open("GET", val, true);
    xhttp.send();
}

function colourSet() {
    sendAJAX("colourMode?r=" + rSlider.value + "&g=" + gSlider.value + "&b=" + bSlider.value);
}