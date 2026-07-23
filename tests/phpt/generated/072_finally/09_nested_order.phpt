--TEST--
nested finally blocks run inside out
--FILE--
<?php
$log = [];
try {
    try { $log[] = 'body'; } finally { $log[] = 'inner'; }
} finally { $log[] = 'outer'; }
echo implode(',', $log);
--EXPECT--
body,inner,outer
