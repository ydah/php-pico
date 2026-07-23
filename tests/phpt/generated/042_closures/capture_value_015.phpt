--TEST--
closure capture-by-value vector 015
--FILE--
<?php
$captured = 108;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(35);
--EXPECT--
143
