--TEST--
closure capture-by-value vector 017
--FILE--
<?php
$captured = 122;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(39);
--EXPECT--
161
