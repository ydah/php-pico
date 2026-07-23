--TEST--
nested array copy-on-write vector 037
--FILE--
<?php
$tree = ['node' => ['value' => 149, 'items' => [150]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 155;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
149:153:150:150,155
