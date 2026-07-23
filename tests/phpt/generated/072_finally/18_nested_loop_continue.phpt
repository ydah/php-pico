--TEST--
finally executes on nested-loop continue
--FILE--
<?php
$log = [];
for ($outer = 0; $outer < 2; $outer++) {
    for ($inner = 0; $inner < 2; $inner++) {
        try { if ($inner === 0) { $log[] = 'skip' . $outer; continue; } $log[] = 'keep' . $outer; }
        finally { $log[] = 'f' . $outer . $inner; }
    }
}
echo implode(',', $log);
--EXPECT--
skip0,f00,keep0,f01,skip1,f10,keep1,f11
