--TEST--
finally executes before loop continue
--FILE--
<?php
$log = [];
for ($i = 0; $i < 3; $i++) {
    try { $log[] = 'try' . $i; if ($i < 2) { continue; } }
    finally { $log[] = 'finally' . $i; }
    $log[] = 'body' . $i;
}
echo implode(',', $log);
--EXPECT--
try0,finally0,try1,finally1,try2,finally2,body2
