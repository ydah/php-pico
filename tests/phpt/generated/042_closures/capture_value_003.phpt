--TEST--
closure capture-by-value vector 003
--FILE--
<?php
$captured = 24;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(11);
--EXPECT--
35
