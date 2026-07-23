--TEST--
closure capture-by-value vector 000
--FILE--
<?php
$captured = 3;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(5);
--EXPECT--
8
