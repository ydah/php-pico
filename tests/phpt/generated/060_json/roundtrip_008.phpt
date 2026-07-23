--TEST--
JSON encode and decode vector 008
--FILE--
<?php
$value = ['name' => 'item08', 'number' => 105, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item08","number":105,"enabled":true}:item08:105:1
