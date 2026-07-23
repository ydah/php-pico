--TEST--
closure capture-by-value vector 001
--FILE--
<?php
$captured = 10;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(7);
--EXPECT--
17
