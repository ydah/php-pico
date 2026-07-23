--TEST--
finally after normal completion
--FILE--
<?php
$log = [];
try { $log[] = 'try'; }
finally { $log[] = 'finally'; }
echo implode(',', $log);
--EXPECT--
try,finally
