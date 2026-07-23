--TEST--
closure capture-by-value vector 002
--FILE--
<?php
$captured = 17;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(9);
--EXPECT--
26
