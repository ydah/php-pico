--TEST--
string split transform and padding vector 028
--FILE--
<?php
$parts = explode(',', 'v28,v29,v30');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX28'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v28|v29|v30:3:V29:mix28:v28--
