--TEST--
closure capture-by-value vector 007
--FILE--
<?php
$captured = 52;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(19);
--EXPECT--
71
