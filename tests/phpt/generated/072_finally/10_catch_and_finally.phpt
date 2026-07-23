--TEST--
catch executes before sibling finally
--FILE--
<?php
$log = [];
try { throw new Exception('x'); }
catch (Exception $error) { $log[] = 'catch'; }
finally { $log[] = 'finally'; }
echo implode(',', $log);
--EXPECT--
catch,finally
