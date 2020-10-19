function patchConfigFiles() {
  try {
    // See also Product.wxs, CustomAction preparePatchConfigFiles.
    // The value of the "CustomActionData" property is:
    // "[INSTALLLOCATION];[DB_OID];[SERVICE_NAME];[WEB_INTERFACE_PORT]"
    log("CustomActionData=" + Session.Property("CustomActionData"));
    var props = Session.Property("CustomActionData").split(";");
    patchConfigFile("config\\ida.par", props);
    return 1;
  } catch (e) {
    return 0;
  }
}

function patchConfigFile(name, props) {
  var path = props[0] + name;
  log("Reading file: " + path);
  var contents = readFile(path);
  //log("Original contents: " + contents);
  contents = replaceVariable(contents, "DB_OID", props[1]);
  contents = replaceVariable(contents, "SERVICE_NAME", props[2]);
  contents = replaceVariable(contents, "WEB_INTERFACE_PORT", props[3]);
  //log("Modified contents: " + contents);
  log("Writing file: " + path);
  writeFile(path, contents);
}

function readFile(path) {
  var fso = new ActiveXObject("Scripting.FileSystemObject");
  var ts = fso.OpenTextFile(path);
  var contents = ts.ReadAll();
  ts.Close();
  return contents;
}

function writeFile(path, contents) {
  var fso = new ActiveXObject("Scripting.FileSystemObject");
  var ts = fso.CreateTextFile(path, true);
  ts.Write(contents);
  ts.Close();
}

function replaceVariable(s, name, value) {
  var variable = "@" + name + "@";
  log("Replacing " + variable + " with '" + value + "'");
  if (value != null) {
    while (s.indexOf(variable) >= 0) {
      s = s.replace(variable, value);
    }
  }
  return s;
}

function log(msg) {
  var msiMessageTypeInfo = 0x04000000;
  var msgrec = Installer.CreateRecord(1);
  msgrec.StringData(0) = "Log: [1]";
  msgrec.StringData(1) = msg;
  Session.Message(msiMessageTypeInfo, msgrec);
}
