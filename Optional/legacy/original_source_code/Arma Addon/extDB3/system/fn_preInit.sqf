diag_log "---------------------------------------------------------------------";
diag_log "---------------------------------------------------------------------";
private  _result = "extDB3" callExtension "9:VERSION";
if (_result == "") then
{
  diag_log "extDB3 Failed to Load, Check Requirements @ https://bitbucket.org/torndeco/extdb3/wiki/Installation";
  diag_log "";
  diag_log "If you are running this on a client, Battleye will random block extensions. Try Disable Battleye";
  extDB3_var_loaded = compileFinal "false";
} else {
  diag_log "extDB3 Loaded";
  extDB3_var_loaded = compileFinal "true";
};
diag_log "---------------------------------------------------------------------";
diag_log "---------------------------------------------------------------------";
