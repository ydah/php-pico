--TEST--
closure capture-by-value vector 005
--FILE--
<?php
$captured = 38;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(15);
--EXPECT--
53
