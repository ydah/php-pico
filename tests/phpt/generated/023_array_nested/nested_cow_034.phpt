--TEST--
nested array copy-on-write vector 034
--FILE--
<?php
$tree = ['node' => ['value' => 137, 'items' => [138]]];
$copy = $tree;
$copy['node']['value'] += 8;
$copy['node']['items'][] = 147;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
137:145:138:138,147
