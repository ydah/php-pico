--TEST--
closure capture-by-value vector 008
--FILE--
<?php
$captured = 59;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(21);
--EXPECT--
80
