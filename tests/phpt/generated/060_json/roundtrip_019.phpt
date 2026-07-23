--TEST--
JSON encode and decode vector 019
--FILE--
<?php
$value = ['name' => 'item19', 'number' => 248, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item19","number":248,"enabled":false}:item19:248:0
