--TEST--
JSON encode and decode vector 018
--FILE--
<?php
$value = ['name' => 'item18', 'number' => 235, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item18","number":235,"enabled":true}:item18:235:1
