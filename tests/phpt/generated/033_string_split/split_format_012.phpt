--TEST--
string split transform and padding vector 012
--FILE--
<?php
$parts = explode(',', 'v12,v13,v14');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX12'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v12|v13|v14:3:V13:mix12:v12--
