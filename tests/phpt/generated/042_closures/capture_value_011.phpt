--TEST--
closure capture-by-value vector 011
--FILE--
<?php
$captured = 80;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(27);
--EXPECT--
107
