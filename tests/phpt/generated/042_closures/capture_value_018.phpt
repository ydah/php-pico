--TEST--
closure capture-by-value vector 018
--FILE--
<?php
$captured = 129;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(41);
--EXPECT--
170
