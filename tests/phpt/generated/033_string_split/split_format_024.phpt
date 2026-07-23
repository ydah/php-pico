--TEST--
string split transform and padding vector 024
--FILE--
<?php
$parts = explode(',', 'v24,v25,v26');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX24'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v24|v25|v26:3:V25:mix24:v24--
