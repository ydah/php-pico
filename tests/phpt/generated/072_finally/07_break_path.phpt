--TEST--
finally executes before loop break
--FILE--
<?php
$log = [];
for ($i = 0; $i < 4; $i++) {
    try { $log[] = 'try' . $i; if ($i === 1) { break; } }
    finally { $log[] = 'finally' . $i; }
}
echo implode(',', $log);
--EXPECT--
try0,finally0,try1,finally1
