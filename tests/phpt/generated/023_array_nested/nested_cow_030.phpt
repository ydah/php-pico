--TEST--
nested array copy-on-write vector 030
--FILE--
<?php
$tree = ['node' => ['value' => 121, 'items' => [122]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 127;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
121:125:122:122,127
