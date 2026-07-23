--TEST--
closure capture-by-value vector 020
--FILE--
<?php
$captured = 143;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(45);
--EXPECT--
188
