var viewportWidth = $(window).width();
var pumpAvg = 0;
var wifi = [  "<img id='wifiIcon' class='ui-btn-left' src='wifi0.png' height='24' width='24' alt='WiFi Very Weak'>",
              "<img id='wifiIcon' class='ui-btn-left' src='wifi1.png' height='24' width='24' alt='WiFi Weak'>",
              "<img id='wifiIcon' class='ui-btn-left' src='wifi2.png' height='24' width='24' alt='WiFi Good'>",
              "<img id='wifiIcon' class='ui-btn-left' src='wifi3.png' height='24' width='24' alt='WiFi Strong'>",
              "<img id='wifiIcon' class='ui-btn-left' src='wifi4.png' height='24' width='24' alt='WiFi Very Strong'>",
              "<img id='wifiIcon' class='ui-btn-left' src='nowifi.png' height='24' width='24' alt='No Connection'>",
            ];

var battery = [ "<img id='batteryIcon' class='ui-btn-right' src='bat20.png' height='24' width='24' alt='Battery Depleted'>",
                "<img id='batteryIcon' class='ui-btn-right' src='bat50.png' height='24' width='24' alt='Battery Low'>",
                "<img id='batteryIcon' class='ui-btn-right' src='bat80.png' height='24' width='24' alt='Battery Good'>",
                "<img id='batteryIcon' class='ui-btn-right' src='bat100.png' height='24' width='24' alt='Battery Full'>",
                "<img id='batteryIcon' class='ui-btn-right' src='batunknown.png' height='24' width='24' alt='Battery Unknown'>",
                "<img id='batteryIcon' class='ui-btn-right' src='plug.png' height='24' width='24' alt='Using AC Power'>",
            ];

$(document).ready(function() {
      var hostName = location.hostname;

      if (hostName==="") hostName="192.168.2.20";

      //alert(hostName);
      /*The user has WebSockets!!! */

      var canvas = document.getElementById("chart");
      canvas.width = $("#gChart").width();

      connect();

      var smoothie = new SmoothieChart({
                          grid: {fillStyle:'rgba(13,39,62,0.78)', strokeStyle:'#4b8da3',
                          lineWidth: 1, millisPerLine: 250, verticalSections: 6, }
      });

      var line1 = new TimeSeries();
      var line2 = new TimeSeries();

      smoothie.streamTo(document.getElementById("chart"), 3000 /*delay*/);
      smoothie.addTimeSeries(line1, { lineWidth:3, strokeStyle:'#00ff00',fillStyle:'rgba(0,0,0,0.30)' });
      smoothie.addTimeSeries(line2, { lineWidth:3,strokeStyle:'#ff8000' });


      function connect(){
          var socket;
          var host = "ws://"+hostName+":81";

          try{
              var socket = new WebSocket(host);

              $("#footerText").html("Connecting...");
              message('<li class="message"><img src="message.png" alt="Message" class="ui-li-icon">Socket Status: '+socket.readyState);

              socket.onopen = function(){
             	  message('<li class="message"><img src="message.png" alt="Message" class="ui-li-icon">Socket Status: '+socket.readyState+' (open)');
                $("#footerText").html("Connected!");
                $("#headerText").html("Pumping Session<br><small>Connected to " + hostName + "</small>");
                setWiFi(3);
                setBattery(5);
              }

              socket.onmessage = function(msg){
               var datamsg = msg.data + '';
               var cmds = datamsg.split("=");

               switch(cmds[0]) {
                case 'pres':
                  var adcvpres = cmds[1];
                  line1.append(new Date().getTime(), adcvpres);
                  line2.append(new Date().getTime(), pumpAvg);
                  break;
                case 'bat':
                  /*$('#adcvbatt').val(msg.data.substring(1, (msg.data.length)));*/
                  break;
                case 'session':
                  if (cmds[1]=="start") {
                    $("#footerText").html("Session Started");
                    message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Session Started');
                  }
                  else if(cmds[1]=="stop") {
                    $("#footerText").html("Session Stopped");
                    message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Session Stopped');
                  }
                  else if(cmds[1]=="complete") {
                    $("#footerText").html("Session Completed");
                    message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Session Completed');
                  }
                  break;
                case 'rest':
                  var mins =parseInt(cmds[1] / 60);
                  var secs =cmds[1]-(mins * 60);
                  if (secs < 10) secs="0"+secs;
                  $("#footerText").html("Session Running<br><small>" +mins +":"+ secs+ " rest time remaining</small>");
                  // message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Start rest period');
                  break;
                case 'remain':
                  var mins =parseInt(cmds[1] / 60);
                  var secs =cmds[1]-(mins * 60);
                  if (secs < 10) secs="0"+secs;
                  $("#footerText").html("Session Running<br><small>" +mins +":"+ secs+ " pump time remaining</small>");
                  break;
                case 'pumping':
                  pumpAvg = cmds[1];
                  break;
                case 'rssi':
                  rssiVal = cmds[1];
                  if (rssiVal <= -80)                       setWiFi(0);
                  else if (rssiVal > -80 && rssiVal <= -65) setWiFi(1);
                  else if (rssiVal > -65 && rssiVal <= -50) setWiFi(2);
                  else if (rssiVal > -50 && rssiVal <= -45) setWiFi(3);
                  else if (rssiVal > -45)                   setWiFi(4);
                  break;
                case 'time':
                  var d = new Date(cmds[1]*1000); // convert unix time to javascript time
                  var local = d.toLocaleTimeString("en-US", {hour: '2-digit', minute:'2-digit', second:'2-digit'});
                  $("#headerText").html(hostName + "<br><small>" + local + " </small>");
                  break;
                case 'boottime':
                    var bootTime = (cmds[1]*1000); // convert unix time to javascript time
                    var d = new Date(bootTime);
                    var local = d.toLocaleDateString("en-US", {month: '2-digit', day: 'numeric', hour: '2-digit', minute:'2-digit'})
                    message('<li class="message"><img src="message.png" alt="Message" class="ui-li-icon">Last reboot: ' + local);
                    break;
                case 'settings':
                  var settings = cmds[1].split(',');

                  $( "#vstart" ).val(settings[0]).slider("refresh");
                  $( "#vend" ).val(settings[1]).slider("refresh");
                  $( "#vdur" ).val(settings[2]).slider("refresh");
                  $( "#vrest" ).val(settings[3]).slider("refresh");
                  $( "#vreps" ).val(settings[4]).slider("refresh");
                  $( "#vpause" ).val(settings[5]).slider("refresh");
                  $( "#vincr" ).val(settings[6]).slider("refresh");

                  message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Settings Updated');
                  break;

                default:
                  message('<li class="message"><img src="message.png" alt="Message" class="ui-li-icon">'+msg.data);
                }
              }


              socket.onclose = function(){
              	message('<li class="action"><img src="error.png" alt="Error" class="ui-li-icon">Socket Status: '+socket.readyState+' (Closed)');
                setWiFi(5);
                setBattery(4);
                $("#footerText").html("Connection lost");
                $("#headerText").html("Pumping Session<br><small>Disconnected</small>");
                setTimeout(connect(), 2000);
              }

          } catch(exception){
             $("#footerText").html("Connection lost");
             $("#headerText").html("Pumping Session<br><small>Disconnected</small>");
             setTimeout(connect(), 2000);
             message('<li class="action"><img src="error.png" alt="Error" class="ui-li-icon">Error: '+exception);
          }

          function setWiFi(level) {
            $("#wifiIcon").replaceWith(wifi[level]);
          }

          function setBattery(level) {
            $("#batteryIcon").replaceWith(battery[level]);
          }

          function message(msg){
            $('#diagList').prepend(msg+'</li>');
            $('#diagList').listview("refresh");
          }


          $(".numbox").on("slidestop", function(){
            var myName = $( this ).attr('name');
            var myValue = $( this ).val();
            var myMsg = myName +"="+ myValue;
            socket.send(myMsg);
            message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Sent: '+myMsg);
          });

          $(":button").click(function(){
            var myName = $( this ).attr('name');
            var myValue = $( this ).html();
            var myAction = myName +"="+ myValue;
            if (myValue=="Start") { /* user clicked start, make sure controller has all the data first */
              $(".numbox").each(function(){
                var myName = $( this ).attr('name');
                var myValue = $( this ).val();
                var myMsg = myName +"="+ myValue;
                socket.send(myMsg);
                message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Sent: '+myMsg);
              });
            }
            socket.send(myAction);
            message('<li class="action"><img src="action.png" alt="Action" class="ui-li-icon">Sent: '+myAction);
            event.preventDefault();
          });

      }//End connect

});
