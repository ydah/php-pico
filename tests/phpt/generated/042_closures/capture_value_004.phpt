--TEST--
closure capture-by-value vector 004
--FILE--
<?php
$captured = 31;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(13);
--EXPECT--
44
