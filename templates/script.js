const socket = io();

const ctx = document.getElementById("waterChart");

const chart = new Chart(ctx,{
type:"line",
data:{
labels:[],
datasets:[{
label:"Water Level %",
data:[]
}]
}
});

socket.on("mqtt_data",(data)=>{

if(data.topic==="water/level")
{
document.getElementById("waterLevel").innerHTML =
data.value + "%";

chart.data.labels.push(
new Date().toLocaleTimeString()
);

chart.data.datasets[0].data.push(
parseInt(data.value)
);

chart.update();
}

if(data.topic==="pump/status")
document.getElementById("pumpStatus").innerHTML =
data.value;

if(data.topic==="light/status")
document.getElementById("lightStatus").innerHTML =
data.value;

if(data.topic==="fan/status")
document.getElementById("fanStatus").innerHTML =
data.value;

});