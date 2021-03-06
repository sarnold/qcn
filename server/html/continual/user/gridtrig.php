<?php
require_once("../inc/util.inc");
require_once("../inc/db.inc");
require_once("../inc/db_ops.inc");

db_init();

$user = get_logged_in_user(true);
// user->donated means they can do download stuff (donated is a SETI@home field reused here)
if (!$user->id || !$user->donated) {
   echo "You are not authorized to use this page.  Please contact a QCN staff member.";
   exit();
}

$query = "select q.id as quakeid, q.time_utc as quake_time, q.magnitude as quake_magnitude, 
q.depth_km as quake_depth, q.latitude as quake_lat,
q.longitude as quake_lon, q.description, q.url, q.guid,
t.id as triggerid, t.hostid, t.ipaddr, t.result_name, t.time_trigger as trigger_time, 
(t.time_received-t.time_trigger) as delay_time, t.time_sync as trigger_sync,
t.sync_offset, t.significance, t.magnitude as trigger_mag, 
t.latitude as trigger_lat, t.longitude as trigger_lon, t.file as trigger_file, t.dt as delta_t,
t.numreset, s.description as sensor_description, t.sw_version, t.qcn_quakeid, t.time_filereq as trigger_timereq, 
t.received_file, t.file_url
FROM
  qcn_trigger t LEFT OUTER JOIN qcn_quake q ON t.qcn_quakeid = q.id
   LEFT JOIN qcn_sensor s ON t.qcn_sensorid = s.id 
";

/*
$query = "select t.id as triggerid, t.hostid, t.ipaddr, t.result_name, t.time_trigger as trigger_time, 
(t.time_received-t.time_trigger) as delay_time, t.time_sync as trigger_sync,
t.sync_offset, t.significance, t.magnitude as trigger_mag, 
t.latitude as trigger_lat, t.longitude as trigger_lon, t.file as trigger_file, t.dt as delta_t,
t.numreset, t.qcn_sensorid, t.sw_version, t.qcn_quakeid, t.time_filereq as trigger_timereq, 
t.received_file, t.file_url
from qcn_trigger t ";
*/

// full querystring
// http://qcn.stanford.edu/continual_dl/trig.php?cbCSV=1&cbUseLat=1&LatMin=-39&LatMax=-30&LonMin=-76&LonMax=-69&cbUseSensor=1&qcn_sensorid=100&cbUseTime=1&date_start=2010-03-24&time_hour_start=0&time_minute_start=0&date_end=2010-03-25&time_hour_end=0&time_minute_end=0&rb_sort=ttd

// sort order options: tta/d  hosta/d  maga/d lata/d lona/d


// first off get the sensor types
$sqlsensor = "select id,description from qcn_sensor order by id";
$result = mysql_query($sqlsensor);
$i = 0;
$arrSensor = array();
if ($result) {
    while ($res = mysql_fetch_array($result)) {
       $arrSensor[$i] = $res;
       $i++;
    }
    mysql_free_result($result);
}

$detail = null;
$show_aggregate = false;

$q = new SqlQueryString();

// start $_GET

$nresults = get_int("nresults", true);
$last_pos = get_int("last_pos", true);

$bUseArchive = get_int("cbUseArchive", true);
$bUseFile  = get_int("cbUseFile", true);
$bUseQuake = get_int("cbUseQuake", true);
$bUseLat   = get_int("cbUseLat", true);
$bUseSensor = get_int("cbUseSensor", true);
$bUseTime  = get_int("cbUseTime", true);
$bUseHost = get_int("cbUseHost", true);
$strHostID = get_int("HostID", true);
$strHostName = get_str("HostName", true);

$quake_mag_min = get_str("quake_mag_min", true);

$qcn_sensorid = get_int("qcn_sensorid", true);
$dateStart = get_str("date_start", true);
$dateEnd   = get_str("date_end", true);

$strLonMin = get_str("LonMin", true);
$strLonMax = get_str("LonMax", true);
$strLatMin = get_str("LatMin", true);
$strLatMax = get_str("LatMax", true);

$timeHourStart   = get_int("time_hour_start", true);
$timeMinuteStart = get_int("time_minute_start", true);

$timeHourEnd   = get_int("time_hour_end", true);
$timeMinuteEnd = get_int("time_minute_end", true);

$sortOrder = get_str("rb_sort", true);

// end $_GET

// make sure these are in the right order, as the sql "between" will fail if max < min!
// people may forget that lon -76 is less than -72 as it may make more sense to think -72 to -76
if ($strLonMax < $strLonMin) {
   $temp = $strLonMax;
   $strLonMax = $strLonMin;
   $strLonMin = $temp;
}

if ($strLatMax < $strLatMin) {
   $temp = $strLatMax;
   $strLatMax = $strLatMin;
   $strLatMin = $temp;
}

if (!$sortOrder) $sortOrder = "ttd";  // triger time desc is default sort order

if ($nresults) {
    $entries_to_show = $nresults;
} else {
    $entries_to_show = 100;
}
$page_entries_to_show = $entries_to_show;

if ($last_pos) {
    $start_at = $last_pos;
} else {
    $start_at = 0;
}

//page_head("QCN Trigger Listing");

echo "<html>
<head>
<link rel=\"STYLESHEET\" type=\"text/css\" href=\"dhtmlx/dhtmlxgrid.css\">
<link rel=\"stylesheet\" type=\"text/css\" href=\"dhtmlx/skins/dhtmlxgrid_dhx_skyblue.css\">
<script  src=\"dhtmlx/dhtmlxcommon.js\"></script>
<script  src=\"dhtmlx/dhtmlxgrid.js\"></script>
<script  src=\"dhtmlx/ext/dhtmlxgrid_nxml.js\"></script>
<script  src=\"dhtmlx/dhtmlxgridcell.js\"></script>

<script type=\"text/javascript\" src=\"calendarDateInput.js\">

/***********************************************
* Jason's Date Input Calendar- By Jason Moon http://calendar.moonscript.com/dateinput.cfm
* Script featured on and available at http://www.dynamicdrive.com
* Keep this notice intact for use.
***********************************************/

</script>
  <title>QCN Trigger Listing</title>
</head>
<body " . BODY_COLOR . ">\n";

 echo "<h5>";
 echo "<table><tr " . TITLE_COLOR . "><td>" . TITLE_FONT . "<font size=\"6\"><b><a href=\"trig.php\">".PROJECT.":</a>  QCN Trigger Listing </b></font></td></tr></table>\n";


// if no constraints then at least use quakes as otherwise we'll have too many i.e. a million triggers
if (!$bUseFile && !$bUseQuake && !$bUseLat && !$bUseTime && !$bUseSensor) {
   $bUseTime = 1;
  /*
   $bUseLat = 1;
   $strLatMin = -39;
   $strLatMax = -30;
   $strLonMin = -76;
   $strLonMax = -69;
   */
}

echo "
<form name='formSelect' method=\"get\" action=gridtrig.php >
<HR>
<BR>
Constraints:<br><br>
  <input type=\"checkbox\" id=\"cbUseFile\" name=\"cbUseFile\" value=\"1\" " . ($bUseFile ? "checked" : "") . "> Only Show If Files Received
<BR><BR>"
//  <input type=\"checkbox\" id=\"cbUseQuake\" name=\"cbUseQuake\" value=\"1\" " . ($bUseQuake ? "checked" : "") . "> Show Matching USGS Quakes 
//  &nbsp&nbsp
//  Minimum Magnitude: <input id=\"quake_mag_min\" name=\"quake_mag_min\" value=\"$quake_mag_min\">
//<BR><BR>
. "  <input type=\"checkbox\" id=\"cbUseLat\" name=\"cbUseLat\" value=\"1\" " . ($bUseLat ? "checked" : "") . "> Use Lat/Lon Constraint (+/- 90 Lat, +/- 180 Lon)
<BR>
  Lat Min: <input id=\"LatMin\" name=\"LatMin\" value=\"" . $strLatMin . "\">
  Lat Max: <input id=\"LatMax\" name=\"LatMax\" value=\"" . $strLatMax . "\">
  Lon Min: <input id=\"LonMin\" name=\"LonMin\" value=\"" . $strLonMin . "\">
  Lon Max: <input id=\"LonMax\" name=\"LonMax\" value=\"" . $strLonMax . "\">
<BR><BR>


  <input type=\"checkbox\" id=\"cbUseSensor\" name=\"cbUseSensor\" value=\"1\" " . ($bUseSensor ? "checked" : "") . "> Use Sensor Constraint

<select name=\"qcn_sensorid\" id=\"qcn_sensorid\">

";
echo "<H5>";

  for ($i = 0; $i < sizeof($arrSensor); $i++)  {
     echo "<option value=" . $arrSensor[$i][0];
     if ($qcn_sensorid == $arrSensor[$i][0]) echo " selected";
     echo ">" . $arrSensor[$i][1] . "\n";
  }

echo "</select>
  <BR><BR>
  <input type=\"checkbox\" id=\"cbUseTime\" name=\"cbUseTime\" value=\"1\" " . ($bUseTime ? "checked" : "") . "> Use Time Constraint
<BR>
";


echo "<ul><table><tr><td>
Start Time (UTC):";

if (!$dateStart) {
  $dateStart = date("Y/m/d", time());  
}
if (!$dateEnd) {
  $dateEnd = date("Y/m/d", time() + (3600*24));  
}

echo "<script>DateInput('date_start', true, 'YYYY-MM-DD', '$dateStart')</script>";

echo "<select name=\"time_hour_start\" id=\"time_hour_start\">
";

for ($i = 0; $i < 24; $i++) {
   echo "<option value=$i ";
   if ($i == $timeHourStart) echo "selected";
   echo ">" . sprintf("%02d", $i);
}

echo "</select>
:
<select name=\"time_minute_start\" id=\"time_minute_start\">";

for ($i = 0; $i < 60; $i++) {
   echo "<option value=$i ";
   if ($i == $timeMinuteStart) echo "selected";
   echo ">" . sprintf("%02d", $i);
}

echo "
</select>

</td><td>

End Time (UTC):";

  echo "<script>DateInput('date_end', true, 'YYYY-MM-DD', '$dateEnd')</script>";

echo "<select name=\"time_hour_end\">
";

for ($i = 0; $i < 24; $i++) {
   echo "<option value=$i ";
   if ($i == $timeHourEnd) echo "selected";
   echo ">" . sprintf("%02d", $i);
}

echo "</select>
:
<select name=\"time_minute_end\" id=\"time_minute_end\">
";

for ($i = 0; $i < 60; $i++) {
   echo "<option value=$i ";
   if ($i == $timeMinuteEnd) echo "selected";
   echo ">" . sprintf("%02d", $i);
}

echo "
</select> </tr></table> </UL>
";

echo "<BR>Sort Order: ";

echo "<H7>";

echo "<select name=\"rb_sort\" id=\"rb_sort\">
";
   echo "<option value=\"maga\" ";
   if ($sortOrder == "maga") echo "selected";
   echo ">Magnitude (Ascending)";

   echo "<option value=\"magd\" ";
   if ($sortOrder == "magd") echo "selected";
   echo ">Magnitude (Descending)";

   echo "<option value=\"tta\" ";
   if ($sortOrder == "tta") echo "selected";
   echo ">Trigger Time (Earliest First)";

   echo "<option value=\"ttd\" ";
   if ($sortOrder == "ttd") echo "selected";
   echo ">Trigger Time (Latest First)";

   echo "<option value=\"lata\" ";
   if ($sortOrder == "lata") echo "selected";
   echo ">Latitude (Ascending)";

   echo "<option value=\"latd\" ";
   if ($sortOrder == "latd") echo "selected";
   echo ">Latitude (Descending)";

   echo "<option value=\"lona\" ";
   if ($sortOrder == "lona") echo "selected";
   echo ">Longitude (Ascending)";

   echo "<option value=\"lond\" ";
   if ($sortOrder == "lond") echo "selected";
   echo ">Longitude (Descending)";

   echo "<option value=\"hosta\" ";
   if ($sortOrder == "hosta") echo "selected";
   echo ">Host ID (Ascending)";

   echo "<option value=\"hostd\" ";
   if ($sortOrder == "hostd") echo "selected";
   echo ">Host ID (Descending)";

   echo "</select>";


// end the form
echo "<BR><BR>
   <input type=\"submit\" value=\"Submit Constraints\" />
   </form> <H7>";

$whereString = "t.varietyid=0 ";

if ($bUseFile) {
   $whereString .= " AND t.received_file = 100 ";
}

if ($bUseQuake) {
   $whereString .= " AND t.qcn_quakeid>0 AND q.magnitude >= " . $quake_mag_min;
}

if ($bUseLat) {
   $whereString .= " AND t.latitude BETWEEN $strLatMin AND $strLatMax AND t.longitude BETWEEN $strLonMin AND $strLonMax ";
}

if ($bUseSensor) {
   $whereString .= " AND t.qcn_sensorid=$qcn_sensorid ";
}

if ($bUseTime) {
   $whereString .= " AND t.time_received BETWEEN unix_timestamp('" . $dateStart . " " . sprintf("%02d", $timeHourStart) . ":" . sprintf("%02d", $timeMinuteStart) . ":00') " 
        . " AND unix_timestamp('" . $dateEnd . " " . sprintf("%02d", $timeHourEnd) . ":" . sprintf("%02d", $timeMinuteEnd) . ":00') ";
}

$sortString = "t.time_trigger DESC";

switch($sortOrder)
{
   case "maga":
      $sortString = "q.magnitude ASC, t.time_trigger DESC";
      break;
   case "magd":
      $sortString = "q.magnitude DESC, t.time_trigger DESC";
      break;
   case "tta":
      $sortString = "t.time_trigger ASC";
      break;
   case "ttd":
      $sortString = "t.time_trigger DESC";
      break;
   case "lata":
      $sortString = "t.latitude ASC, t.longitude ASC";
      break;
   case "latd":
      $sortString = "t.latitude DESC, t.longitude DESC";
      break;
   case "lona":
      $sortString = "t.longitude ASC, t.latitude ASC";
      break;
   case "lond":
      $sortString = "t.longitude DESC, t.latitude DESC";
      break;
   case "hosta":
      $sortString = "t.hostid ASC";
      break;
   case "hostd":
      $sortString = "t.hostid DESC";
      break;
}

$query .= " WHERE " . $whereString . " ORDER BY " . $sortString;

//print "<BR><BR>$query<BR><BR>";

echo "
<script type=\"text/javascript\">
   
function SetAllCheckBoxes(FormName, FieldName, CheckValue)
{
	if(!document.forms[FormName])
		return;
	var objCheckBoxes = document.forms[FormName].elements[FieldName];
	if(!objCheckBoxes)
		return;
	var countCheckBoxes = objCheckBoxes.length;
	if(!countCheckBoxes)
		objCheckBoxes.checked = CheckValue;
	else
		// set the check value for all check boxes
		for(var i = 0; i < countCheckBoxes; i++)
	           if (! objCheckBoxes[i].disabled)
                      objCheckBoxes[i].checked = CheckValue;
}
</script>
<HR>
";

/*
select q.id as quakeid, q.time_utc as quake_time, q.magnitude as quake_magnitude, 
q.depth_km as quake_depth, q.latitude as quake_lat,
q.longitude as quake_lon, q.description, q.url, q.guid,
t.id as triggerid, t.hostid, t.ipaddr, t.result_name, t.time_trigger as trigger_time, 
(t.time_received-t.time_trigger) as delay_time, t.time_sync as trigger_sync,
t.sync_offset, t.significance, t.magnitude as trigger_mag, 
t.latitude as trigger_lat, t.longitude as trigger_lon, t.file as trigger_file, t.dt as delta_t,
t.numreset, s.description as sensor_description, t.sw_version, t.qcn_quakeid, t.time_filereq as trigger_timereq, 
t.received_file, t.file_url

*/

   // tmp file name tied to user ID & server time
   $fileTemp = sprintf("data/%ld_user%08d.csv", time(), $user->id);
   $ftmp = fopen($fileTemp, "w");
   $header = "QuakeID, QuakeTimeUTC, QuakeMagnitude, QuakeDepth, QuakeLat, QuakeLon, QuakeDesc, QuakeURL, QuakeGUID, "
            . "TriggerID, HostID, IPAddr, ResultName, TriggerTime, DelayTime, SyncTime, SyncOffset, Significance, Magnitude, Lat, Lon, File, DT, "
            . "NumReset, Sensor, SWVersion, USGSQuakeID, TimeFileReq, ReceivedFile, FileURL";

   if ($ftmp) {  
      // header line 
      //fwrite($ftmp, $tmpstr);

      // build data csv file then load
      $result = mysql_query($query);
      $i = 0;
      if ($result) {
        while ($res = mysql_fetch_array($result)) {
           $tmpstr = "";
           for ($j = 1; $j <= 30; $j++) {
              $tmpstr .= $res[$j];
              if ($j < 30) {
                 $tmpstr .= ",";
              }
           }
           $tmpstr .= "\n";
           fwrite($ftmp, $tmpstr);
           $i++;
         }
      }
      mysql_free_result($result);
      fclose($ftmp); 
    }

echo "
<table width=\"800\" cellspacing=\"0\" cellpadding=\"0\">
    <tr>
        <td valign=\"top\">
            <div id=\"gridbox\" style=\"width:800px; height:600px; background-color:white;\"></div>
        </td>
    </tr>
</table>
<br>

<script>
mygrid = new dhtmlXGridObject('gridbox');
mygrid.setImagePath(\"dhtmlx/imgs/\");
mygrid.setHeader(\"$header\");
//mygrid.setHeader(\"Sales,Book Title,Author,Price,In Store,Shipping,Bestseller,Date of Publication\");
//mygrid.setInitWidths(\"50,150,100,80,80,80,80,200\");
//mygrid.setColAlign(\"right,left,left,right,center,left,center,center\");
//mygrid.setColTypes(\"dyn,ed,ed,price,ch,co,ra,r\");
//mygrid.getCombo(5).put(2, 2);
mygrid.init();
mygrid.setSkin(\"dhx_skyblue\");
mygrid.enableCSVAutoID(true);
mygrid.loadCSVFile(\"$fileTemp\");
</script>
<BR>
";
 
 echo "<BR><BR><A HREF=\"" . $fileTemp . "\">Download CSV/Text File Here (File Size " . sprintf("%7.2f", (filesize($fileTemp) / 1e6)) . " MB)</A><BR><BR>";

page_tail();


function qcn_trigger_header() {
    echo "
        <tr>
        <th>Request?</th>
        <th>ID</th>
        <th>HostID</th>
        <th>IP Addr</th>
        <th>Result</th>
        <th>TimeTrig</th>
        <th>Delay(s)</th>
        <th>TimeSync</th>
        <th>SyncOffset(s)</th>
        <th>Magnitude</th>
        <th>Significance</th>
        <th>Latitude</th>
        <th>Longitude</th>
        <th>NumReset</th>
        <th>DT</th>
        <th>Sensor</th>
        <th>Version</th>
        <th>Time File Req</th>
        <th>Received File</th>
        <th>File Download</th>
        <th>USGS ID</th>
        <th>Quake Magnitude</th>
        <th>Quake Time (UTC)</th>
        <th>Quake Latitude</th>
        <th>Quake Longitude</th>
        <th>Quake Description</th>
        <th>USGS GUID</th>
        </tr>
    ";
}


function qcn_trigger_detail($res) 
{
    $sensor_type = $res->sensor_description;
    echo "
        <tr>
        <td><input type=\"checkbox\" name=\"cb_reqfile[]\" id=\"cb_reqfile[]\" value=\"$res->triggerid\"" . 
       ($res->received_file == 100 || $res->trigger_timereq>0 ? " disabled " : " " ) . 
       "></td>
        <td>$res->triggerid</td>
        <td><a href=\"db_action.php?table=host&id=$res->hostid\">" . host_name_by_id($res->hostid) . "</a></td>
        <td>$res->ipaddr</td>
        <td>$res->result_name</td>
        <td>" . time_str($res->trigger_time) . "</td>
        <td>" . round($res->delay_time, 2) . "</td>
        <td>" . time_str($res->trigger_sync) . "</td>
        <td>$res->sync_offset</td>
        <td>$res->trigger_mag</td>
        <td>$res->significance</td>
        <td>" . round($res->trigger_lat,4) . "</td>
        <td>" . round($res->trigger_lon,4) . "</td>
        <td>" . ($res->numreset ? $res->numreset : 0) . "</td>
        <td>$res->delta_t</td>
        <td>$sensor_type</td>
        <td>$res->sw_version</td>";
        
        echo "
        <td>" . time_str($res->trigger_timereq) . "</td>
        <td>" . ($res->received_file == 100 ? " Yes " : " No " ) . "</td>";

        if ($res->file_url) {
          echo "<td><a href=\"" . $res->file_url . "\">Download</a></td>";
        }
        else {
          echo "<td>N/A</td>";
        }

        if ($res->qcn_quakeid) {
           echo "<td><a href=\"db_action.php?table=qcn_quake&id=$res->qcn_quakeid\">$res->qcn_quakeid</a></td>";
           echo "<td>$res->quake_magnitude</td>";
           echo "<td>" . time_str($res->quake_time) . "</td>";
           echo "<td>$res->quake_lat</td>";
           echo "<td>$res->quake_lon</td>";
           echo "<td>$res->description</td>";
           echo "<td>$res->guid</td>";
        }
        else {
           echo "<td>N/A</td>";
           echo "<td>&nbsp</td>";
           echo "<td>&nbsp</td>";
           echo "<td>&nbsp</td>";
           echo "<td>&nbsp</td>";
           echo "<td>&nbsp</td>";
           echo "<td>&nbsp</td>";
        }

    echo "</tr>
    ";
}


?>
