--TEST--
closure capture-by-value vector 016
--FILE--
<?php
$captured = 115;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(37);
--EXPECT--
152
