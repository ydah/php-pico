--TEST--
finally executes on an inner nested-loop break
--FILE--
<?php
$log = [];
for ($outer = 0; $outer < 2; $outer++) {
    for ($inner = 0; $inner < 3; $inner++) {
        try { $log[] = $outer . $inner; if ($inner === 1) { break; } }
        finally { $log[] = 'f' . $outer . $inner; }
    }
}
echo implode(',', $log);
--EXPECT--
00,f00,01,f01,10,f10,11,f11
