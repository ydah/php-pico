--TEST--
associative key order vector 026
--FILE--
<?php
$items = ['left' => 36, 4 => 98, 'right' => 160];
unset($items[4]);
$items['tail'] = 161;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:36,160,161:1
