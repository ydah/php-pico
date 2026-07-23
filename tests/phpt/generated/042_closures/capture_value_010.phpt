--TEST--
closure capture-by-value vector 010
--FILE--
<?php
$captured = 73;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(25);
--EXPECT--
98
