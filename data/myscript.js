var viewportWidth = $(window).width();
var pumpAvg = 0;

$(document).ready(function() {
      var hostName = location.hostname;
      
      if (hostName==="") hostName="192.168.2.108";

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
              message('<li class="event">Socket Status: '+socket.readyState);

              socket.onopen = function(){
             	 message('<li class="event">Socket Status: '+socket.readyState+' (open)');
               $("#footerText").html("Connected!");
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
                  if (cmds[1]=="start") $("#footerText").html("Session Started");
                  else if(cmds[1]=="stop") $("#footerText").html("Session Stopped");
                  else if(cmds[1]=="complete") $("#footerText").html("Session Completed");
                  break;
                case 'rest':
                  var mins =parseInt(cmds[1] / 60);
                  var secs =cmds[1]-(mins * 60);
                  if (secs < 10) secs="0"+secs;
                  $("#footerText").html("Session Running<br>" +mins +":"+ secs+ " rest time remaining");
                  break;
                case 'remain':
                  var mins =parseInt(cmds[1] / 60);
                  var secs =cmds[1]-(mins * 60);
                  if (secs < 10) secs="0"+secs;
                  $("#footerText").html("Session Running<br>" +mins +":"+ secs+ " pump time remaining");
                  break;
                case 'pumping':
                  pumpAvg = cmds[1];
                  break;
                case 'settings':
                  var settings = cmds[1].split(',');

                  $( "#vstart" ).val(settings[0]).slider("refresh");
                  $( "#vend" ).val(settings[1]).slider("refresh");
                  $( "#vdur" ).val(settings[2]).slider("refresh");
                  $( "#vrest" ).val(settings[3]).slider("refresh");
                  $( "#vreps" ).val(settings[4]).slider("refresh");

                  message('<li class="message">Settings Updated'); 
                  break;

                default:
                  message('<li class="message">Msg: '+msg.data); 
                }
              }
              

              socket.onclose = function(){
              	message('<li class="event">Socket Status: '+socket.readyState+' (Closed)');
                 $("#footerText").html("Connection lost!");
                 setTimeout(connect(), 2000);
              }			

          } catch(exception){
             message('<li>Error'+exception);
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
            message('<li class="message">Sent: '+myMsg);
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
                message('<li message="event">Sent: '+myMsg);
              });
            }
            socket.send(myAction);
            message('<li class="message">Sent: '+myAction);
            event.preventDefault();
          });

      }//End connect

});